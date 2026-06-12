#include "M1Enemy.h"

#include "M1Audio.h"
#include "M1Tuning.h"

#include "M1MonsterTable.h"
#include "M1Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/DamageEvents.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Sense cadence: LOS traces are the only per-enemy cost that matters with
	// 300+ monsters in a level, so they run on this staggered interval (with a
	// random initial offset), never every frame.
	constexpr float GSenseIntervalMin = 0.30f;
	constexpr float GSenseIntervalMax = 0.50f;
}

const FM1MonsterRow* AM1Enemy::FindRow(int32 MonsterType)
{
	return M1FindMonster(MonsterType);
}

AM1Enemy::AM1Enemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// Marathon AI is direct steering -- explicitly no AIController/navmesh.
	AutoPossessAI = EAutoPossessAI::Disabled;
	bUseControllerRotationYaw = false;

	// Placeholder capsule; InitFromType sets the real per-type size. The capsule
	// must block Visibility so the player's hitscan weapons can hit enemies
	// (the default Pawn profile ignores that channel).
	GetCapsuleComponent()->InitCapsuleSize(20.0f, 40.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		// No controller is ever possessed: this flag makes CharacterMovement
		// consume AddMovementInput (and apply gravity) anyway.
		Move->bRunPhysicsWithNoController = true;
		Move->bOrientRotationToMovement = false;  // rotation = billboard only
		Move->MaxWalkSpeed = 150.0f;              // per-type in InitFromType
		Move->MaxAcceleration = 1024.0f;
		Move->BrakingDecelerationWalking = 1024.0f;
		Move->MaxStepHeight = 35.0f;              // Marathon stairs are ~30cm
		Move->SetWalkableFloorAngle(46.0f);
	}

	// Sprite card: the engine's 100x100 XY plane (normal +Z, U along +X,
	// V along +Y). Roll +90 then yaw -90 stands it upright with its normal on
	// actor +X (the billboard yaw points +X at the camera every tick) and the
	// image top up. Scaled per type in InitFromType.
	SpritePlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpritePlane"));
	SpritePlane->SetupAttachment(GetCapsuleComponent());
	SpritePlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpritePlane->SetCollisionProfileName(TEXT("NoCollision"));
	SpritePlane->SetCastShadow(false);
	SpritePlane->SetRelativeRotation(FRotator(0.0f, -90.0f, 90.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		SpritePlane->SetStaticMesh(PlaneFinder.Object);
	}

	// Optional: tolerate the material not existing yet (sprites not imported);
	// the enemy then renders with the default world material instead of crashing.
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> MatFinder(
		TEXT("/Game/Sprites/M_M1Sprite.M_M1Sprite"));
	SpriteMaterial = MatFinder.Get();
}

bool AM1Enemy::InitFromType(int32 MonsterType)
{
	Row = FindRow(MonsterType);
	if (!Row)
	{
		return false;
	}

	HitPoints = Row->Vitality;

	// Capture the spawn facing for view selection before Tick starts overwriting
	// the actor yaw with the camera billboard.
	FacingYawDeg = GetActorRotation().Yaw;

	// Capsule: data radius/half-height (half-height clamped so the capsule stays
	// valid for squat types like Lookers).
	const float HalfHeight = FMath::Max(Row->HeightCm * 0.5f, Row->RadiusCm);
	GetCapsuleComponent()->SetCapsuleSize(Row->RadiusCm, HalfHeight);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = Row->SpeedCmS;
	}

	// --- Sprites. Each row carries view-major shared-texture grids for the
	// stand/walk/dead states; the actual textures are loaded lazily + cached in
	// GetSharedTexture and chosen per (view, frame) in Tick. ---
	TextureCache.Reset();
	CurrentTexture = nullptr;

	if (SpritePlane)
	{
		// World size from the sprite's Marathon world box; the plane is 100x100,
		// so scale = size/100. Feet sit at the capsule bottom. Cache the |scale|
		// per axis so the billboard horizontal flip can toggle scale Y's sign
		// without losing the magnitude.
		FlipScaleX = Row->SpriteWidthCm / 100.0f;
		FlipScaleY = Row->SpriteHeightCm / 100.0f;  // plane local Y after roll+yaw
		FlipScaleZ = 1.0f;
		FlipSign = 0;
		SpritePlane->SetRelativeScale3D(FVector(FlipScaleX, FlipScaleY, FlipScaleZ));
		SpritePlane->SetRelativeLocation(FVector(
			0.0f, 0.0f, Row->SpriteHeightCm * 0.5f - HalfHeight + 1.0f));
		if (SpriteMaterial)
		{
			SpriteMID = UMaterialInstanceDynamic::Create(SpriteMaterial, this);
			if (SpriteMID)
			{
				SpritePlane->SetMaterial(0, SpriteMID);
			}
		}
	}
	// Initial pose: stand, frame 0 (view chosen next tick once the camera moves).
	UpdateSpriteForState(Row->Stand, 0);

	// --- Sounds (empty path = silent). ---
	ActivationSound = (Row->ActivationSound && Row->ActivationSound[0])
		? LoadObject<USoundBase>(nullptr, Row->ActivationSound) : nullptr;
	RandomSound = (Row->RandomSound && Row->RandomSound[0])
		? LoadObject<USoundBase>(nullptr, Row->RandomSound) : nullptr;
	DeathSound = (Row->DeathSound && Row->DeathSound[0])
		? LoadObject<USoundBase>(nullptr, Row->DeathSound) : nullptr;
	ProjFlybySound = (Row->ProjFlybySound && Row->ProjFlybySound[0])
		? LoadObject<USoundBase>(nullptr, Row->ProjFlybySound) : nullptr;
	MeleeSound = (Row->MeleeSound && Row->MeleeSound[0])
		? LoadObject<USoundBase>(nullptr, Row->MeleeSound) : nullptr;

	SnapToFloor();

	// Random initial sense offset: staggers LOS traces across the population.
	if (UWorld* World = GetWorld())
	{
		NextSenseTime = World->GetTimeSeconds() + FMath::FRandRange(0.0f, GSenseIntervalMax);
	}
	return true;
}

void AM1Enemy::SnapToFloor()
{
	// Placement Z is relative to the polygon FLOOR (usually 0), so the spawn
	// point is at/below the surface. Trace down from a bit above and rest the
	// capsule on the first world-static hit (sector meshes are complex-as-simple
	// collision); pawns/corpses are deliberately not floor.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Loc = GetActorLocation();
	const FVector Start = Loc + FVector(0.0f, 0.0f, 200.0f);
	const FVector End = Loc - FVector(0.0f, 0.0f, 2000.0f);
	const FCollisionObjectQueryParams ObjParams(ECC_TO_BITFIELD(ECC_WorldStatic));
	FCollisionQueryParams Params(FName(TEXT("M1EnemyFloorSnap")), true, this);
	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, Start, End, ObjParams, Params))
	{
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		SetActorLocation(FVector(Loc.X, Loc.Y, Hit.ImpactPoint.Z + HalfHeight + 2.0f),
			false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AM1Enemy::SetSpriteTexture(UTexture2D* Texture)
{
	if (Texture && SpriteMID && Texture != CurrentTexture)
	{
		SpriteMID->SetTextureParameterValue(TEXT("Sprite"), Texture);
		CurrentTexture = Texture;
	}
}

UTexture2D* AM1Enemy::GetSharedTexture(const TCHAR* Path)
{
	if (!Path || !Path[0])
	{
		return nullptr;
	}
	const FString Key(Path);
	// A cached null means "already tried, not imported yet" -- don't retry.
	if (UTexture2D** Found = TextureCache.Find(Key))
	{
		return *Found;
	}
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, Path);
	TextureCache.Add(Key, Tex);
	return Tex;
}

UTexture2D* AM1Enemy::PickViewTexture(const FM1SpriteState& StateData, int32 Frame,
	bool& bOutFlip) const
{
	bOutFlip = false;
	const int32 NumViews = StateData.NumViews;
	const int32 FramesPerView = StateData.FramesPerView;
	if (NumViews <= 0 || FramesPerView <= 0 || !StateData.Textures)
	{
		return nullptr;
	}

	// Direction index d in 0..7 from the camera's bearing relative to the enemy's
	// FACING yaw. d=0 = camera sees the enemy's FRONT. The actor yaw is kept
	// pointed at the player while chasing (see Tick), so strafing around the
	// enemy cycles through its side/back views.
	int32 ViewIndex = 0;
	bool bFlip = false;
	if (NumViews == 1)
	{
		ViewIndex = 0;  // single-view shape: always view 0, never flipped
	}
	else if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(
		const_cast<AM1Enemy*>(this), 0))
	{
		const FVector ToCam = Cam->GetCameraLocation() - GetActorLocation();
		const float CamYaw = FMath::RadiansToDegrees(FMath::Atan2(ToCam.Y, ToCam.X));
		// Relative bearing: how far the camera sits off the enemy's facing.
		const float Rel = FRotator::NormalizeAxis(CamYaw - FacingYawDeg);
		// d = round(rel / 45) mod 8; 0 = front. Add 360 before mod for negatives.
		const int32 d = ((FMath::RoundToInt(Rel / 45.0f) % 8) + 8) % 8;
		// d -> (view, mirror): 0..4 map straight; 5,6,7 mirror views 3,2,1.
		static const int32 ViewForDir[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
		static const bool  FlipForDir[8] = { false, false, false, false,
		                                     false, true, true, true };
		ViewIndex = ViewForDir[d];
		bFlip = FlipForDir[d];
	}

	// Clamp to the shape's real view count (4-view shapes have no view 4, etc.).
	ViewIndex = FMath::Clamp(ViewIndex, 0, NumViews - 1);
	const int32 ClampedFrame = (Frame % FramesPerView + FramesPerView) % FramesPerView;
	const int32 Flat = ViewIndex * FramesPerView + ClampedFrame;  // view-major

	bOutFlip = bFlip;
	return const_cast<AM1Enemy*>(this)->GetSharedTexture(StateData.Textures[Flat]);
}

void AM1Enemy::UpdateSpriteForState(const FM1SpriteState& StateData, int32 Frame)
{
	bool bFlip = false;
	UTexture2D* Tex = PickViewTexture(StateData, Frame, bFlip);

	// Horizontal flip: the sprite plane's local Y is its horizontal axis (after
	// the ctor's roll+90/yaw-90). Flip by negating scale Y; the material is
	// two-sided so the back face shows. Only write when the sign changes.
	const int32 WantSign = bFlip ? -1 : 1;
	if (SpritePlane && WantSign != FlipSign)
	{
		FlipSign = WantSign;
		SpritePlane->SetRelativeScale3D(FVector(
			FlipScaleX, FlipScaleY * WantSign, FlipScaleZ));
	}

	// Tolerate a missing (not-yet-imported) texture: keep the last sprite.
	if (Tex)
	{
		SetSpriteTexture(Tex);
	}
}

bool AM1Enemy::HasLineOfSight(const FVector& TargetLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// World-static only: walls/floors block sight, other monsters don't.
	const FVector Eye = GetActorLocation()
		+ FVector(0.0f, 0.0f, GetSimpleCollisionHalfHeight() * 0.5f);
	const FCollisionObjectQueryParams ObjParams(ECC_TO_BITFIELD(ECC_WorldStatic));
	FCollisionQueryParams Params(FName(TEXT("M1EnemyLOS")), false, this);
	return !World->LineTraceTestByObjectType(Eye, TargetLocation, ObjParams, Params);
}

void AM1Enemy::Activate(float Now)
{
	State = EM1EnemyState::Chase;
	if (ActivationSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, ActivationSound, GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
	}
	NextRandomSoundTime = Now + FMath::FRandRange(4.0f, 8.0f);
	NextAttackTime = Now + M1Tuning::GFirstAttackDelay;  // reaction beat before first attack
}

void AM1Enemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!Row || !World)
	{
		return;
	}

	// --- Yaw-only billboard at the local player's camera (cheap: no traces). ---
	// IMPORTANT: the actor yaw is the BILLBOARD (always faces the camera); the
	// enemy's logical facing for view selection is the separate FacingYawDeg,
	// updated below while chasing. PickViewTexture reads FacingYawDeg, never the
	// actor yaw, so the two never fight.
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCam = Cam->GetCameraLocation() - GetActorLocation();
		if (!ToCam.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.0f,
				FMath::RadiansToDegrees(FMath::Atan2(ToCam.Y, ToCam.X)), 0.0f));
		}
	}

	if (State == EM1EnemyState::Dead)
	{
		// Corpse: keep billboarding the dead sprite from the right view angle.
		UpdateSpriteForState(Row->Dead, 0);
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const FVector MyLoc = GetActorLocation();
	const FVector PlayerLoc = Player->GetActorLocation();
	const float Dist = FVector::Dist(MyLoc, PlayerLoc);

	if (State == EM1EnemyState::Idle)
	{
		// Idle keeps its spawn facing; show stand frame 0 from the camera's view.
		UpdateSpriteForState(Row->Stand, 0);
		// Cheap distance gate first; the LOS trace only runs on the staggered
		// sense timer and only when the player is inside visual range.
		if (Now >= NextSenseTime)
		{
			NextSenseTime = Now + FMath::FRandRange(GSenseIntervalMin, GSenseIntervalMax);
			if (Dist <= Row->VisualRangeCm && HasLineOfSight(PlayerLoc))
			{
				bHasLOS = true;
				Activate(Now);
			}
		}
		return;
	}

	// --- Chasing: face the player so strafing reveals side/back views. ---
	{
		FVector ToPlayer = PlayerLoc - MyLoc;
		ToPlayer.Z = 0.0f;
		if (!ToPlayer.IsNearlyZero())
		{
			FacingYawDeg = FMath::RadiansToDegrees(FMath::Atan2(ToPlayer.Y, ToPlayer.X));
		}
	}

	// --- Chase ---
	if (Now >= NextSenseTime)
	{
		NextSenseTime = Now + FMath::FRandRange(GSenseIntervalMin, GSenseIntervalMax);
		bHasLOS = HasLineOfSight(PlayerLoc);
	}

	if (RandomSound && Now >= NextRandomSoundTime)
	{
		NextRandomSoundTime = Now + FMath::FRandRange(4.0f, 8.0f);
		if (FMath::FRand() < 0.3f)
		{
			UGameplayStatics::SpawnSoundAtLocation(this, RandomSound, MyLoc, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
		}
	}

	// Attack windows. Melee fires inside its reach; ranged inside its (capped)
	// range with LOS. The closest applicable range gates the steering stop.
	const float MeleeRange = Row->bHasMelee
		? FMath::Max(Row->MeleeRangeCm, Row->RadiusCm + M1Tuning::GMeleeReachCm) : 0.0f;
	const float RangedRange = Row->bRanged
		? FMath::Min(Row->AttackRangeRangedCm, M1Tuning::GRangedRangeCapCm) : 0.0f;

	const bool bMeleeNow = Row->bHasMelee && Row->MeleeDamage > 0.0f
		&& Dist <= MeleeRange;
	const bool bRangedNow = Row->bRanged && Row->ProjDamage > 0.0f
		&& Dist <= RangedRange && bHasLOS;

	if ((bMeleeNow || bRangedNow) && Now >= NextAttackTime)
	{
		NextAttackTime = Now + Row->AttackIntervalS;
		// Prefer melee when the player is close enough for it; else the bolt.
		PerformAttack(Player, bMeleeNow);
	}

	// Stop closing once inside whichever attack range applies (the larger of the
	// two when the enemy has both, so it can still shoot from range).
	const float StopRange = FMath::Max(MeleeRange, RangedRange);

	// Direct Marathon steering: straight at the player on XY; when blocked
	// (chasing but barely moving), veer 45 degrees for half a second.
	if (Dist > StopRange * 0.8f)
	{
		FVector Dir = PlayerLoc - MyLoc;
		Dir.Z = 0.0f;
		Dir = Dir.GetSafeNormal();
		if (!Dir.IsNearlyZero())
		{
			const bool bBlocked = GetVelocity().SizeSquared2D() < 10.0f * 10.0f;
			if (Now < SidestepEndTime)
			{
				Dir = Dir.RotateAngleAxis(45.0f * SidestepSign, FVector::UpVector);
			}
			else if (bBlocked && Dist > 150.0f)
			{
				SidestepEndTime = Now + 0.5f;
				SidestepSign = FMath::RandBool() ? 1.0f : -1.0f;
				Dir = Dir.RotateAngleAxis(45.0f * SidestepSign, FVector::UpVector);
			}
			AddMovementInput(Dir);
		}
	}

	// --- Walk animation: advance the walk grid's frame on its own timer while
	// actually moving; fall back to the stand pose when stopped. Both states
	// pick their (view, frame) from the camera angle every tick. ---
	const bool bMoving = GetVelocity().SizeSquared2D() > 10.0f * 10.0f;
	if (bMoving && Row->Walk.FramesPerView > 1)
	{
		AnimTime += DeltaSeconds;
		const float FrameTime = FMath::Max(Row->Walk.FrameTicks / 30.0f, 0.033f);
		AnimFrame = static_cast<int32>(AnimTime / FrameTime);
		UpdateSpriteForState(Row->Walk, AnimFrame);
	}
	else if (bMoving)
	{
		UpdateSpriteForState(Row->Walk, 0);  // single-frame walk
	}
	else
	{
		AnimFrame = 0;
		AnimTime = 0.0f;
		UpdateSpriteForState(Row->Stand, 0);
	}
}

void AM1Enemy::PerformAttack(APawn* Player, bool bMelee)
{
	UWorld* World = GetWorld();
	if (!World || !Player || !Row)
	{
		return;
	}

	// Show the stand pose for the attack beat (no dedicated attack frames in v1);
	// the walk cycle resumes on the next animated tick.
	AnimFrame = 0;
	UpdateSpriteForState(Row->Stand, 0);

	if (bMelee)
	{
		// Melee: the in-range check just happened in Tick; land the hit.
		UGameplayStatics::ApplyDamage(Player, Row->MeleeDamage, GetController(), this, nullptr);
		if (MeleeSound)
		{
			UGameplayStatics::SpawnSoundAtLocation(this, MeleeSound, GetActorLocation(),
				FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
		}
		return;
	}

	// Ranged: spawn an AM1Projectile from the chest aimed at the player's camera
	// (so it leads the eye, not the feet). Instigator = this enemy so it ignores
	// its own shooter; it CAN still hit other Pfhor (Marathon infighting -- no
	// friendly-fire prevention here, by design).
	const FVector Muzzle = GetActorLocation()
		+ FVector(0.0f, 0.0f, Row->HeightCm * 0.6f);
	FVector AimAt = Player->GetActorLocation();
	if (APlayerCameraManager* PlayerCam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		AimAt = PlayerCam->GetCameraLocation();
	}
	const FVector Dir = (AimAt - Muzzle).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLoc = Muzzle + Dir * (Row->RadiusCm + 20.0f);
	if (AM1Projectile* Proj = World->SpawnActor<AM1Projectile>(
		AM1Projectile::StaticClass(), SpawnLoc, Dir.Rotation(), SpawnParams))
	{
		// Flyby sound doubles as the impact SFX in v1 (no dedicated impact id).
		Proj->Init(Row->ProjSpeedCmps, Row->ProjDamage, Row->ProjBlastCm, ProjFlybySound);
	}

	// Fire/launch SFX (attenuated) at the muzzle.
	if (ProjFlybySound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, ProjFlybySound, Muzzle,
			FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
	}
}

float AM1Enemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!Row || State == EM1EnemyState::Dead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	HitPoints -= DamageAmount;

	// Getting shot wakes a monster even without line of sight (Marathon-true).
	if (State == EM1EnemyState::Idle)
	{
		if (UWorld* World = GetWorld())
		{
			Activate(World->GetTimeSeconds());
		}
	}

	if (HitPoints <= 0.0f)
	{
		Die();
	}
	return DamageAmount;
}

void AM1Enemy::Die()
{
	if (State == EM1EnemyState::Dead)
	{
		return;
	}
	State = EM1EnemyState::Dead;

	if (DeathSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, DeathSound, GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	// Corpses neither block movement nor soak hitscan fire, and persist for the
	// rest of the level (Marathon keeps its corpses). Tick keeps billboarding
	// the dead sprite, nothing else.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCanBeDamaged(false);
	// Show the dead pose now; Tick keeps re-picking its view as the camera moves.
	AnimFrame = 0;
	UpdateSpriteForState(Row->Dead, 0);
}
