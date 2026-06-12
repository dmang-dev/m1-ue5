#include "M1Enemy.h"

#include "M1Audio.h"

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
	constexpr float GMeleeReachCm = 60.0f;     // melee range = capsule radius + this
	constexpr float GRangedRangeCapCm = 1500.0f;
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

	// Capsule: data radius/half-height (half-height clamped so the capsule stays
	// valid for squat types like Lookers).
	const float HalfHeight = FMath::Max(Row->HeightCm * 0.5f, Row->RadiusCm);
	GetCapsuleComponent()->SetCapsuleSize(Row->RadiusCm, HalfHeight);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = Row->SpeedCmS;
	}

	// --- Sprites. Asset paths follow the generated SpriteBase convention:
	// /Game/Sprites/Monsters/T_mon<N>  +  _stand/_move<i>/_dead. ---
	const FString Base(Row->SpriteBase);
	int32 SlashIdx = INDEX_NONE;
	Base.FindLastChar(TEXT('/'), SlashIdx);
	const FString Leaf = (SlashIdx != INDEX_NONE) ? Base.Mid(SlashIdx + 1) : Base;
	auto LoadTex = [&Base, &Leaf](const FString& Suffix) -> UTexture2D*
	{
		const FString Path = FString::Printf(TEXT("%s%s.%s%s"), *Base, *Suffix, *Leaf, *Suffix);
		return LoadObject<UTexture2D>(nullptr, *Path);
	};

	StandTexture = LoadTex(TEXT("_stand"));
	DeadTexture = LoadTex(TEXT("_dead"));
	MoveTextures.Reset();
	for (int32 i = 0; i < Row->MoveFrames; ++i)
	{
		if (UTexture2D* Frame = LoadTex(FString::Printf(TEXT("_move%d"), i)))
		{
			MoveTextures.Add(Frame);
		}
	}
	if (!StandTexture && MoveTextures.Num() > 0)
	{
		StandTexture = MoveTextures[0];
	}
	if (!DeadTexture)
	{
		DeadTexture = StandTexture;
	}

	if (SpritePlane)
	{
		// World size from the sprite's Marathon world box; the plane is 100x100,
		// so scale = size/100. Feet sit at the capsule bottom.
		SpritePlane->SetRelativeScale3D(FVector(
			Row->SpriteWidthCm / 100.0f, Row->SpriteHeightCm / 100.0f, 1.0f));
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
	SetSpriteTexture(StandTexture);

	// --- Sounds (empty path = silent). ---
	ActivationSound = (Row->ActivationSound && Row->ActivationSound[0])
		? LoadObject<USoundBase>(nullptr, Row->ActivationSound) : nullptr;
	RandomSound = (Row->RandomSound && Row->RandomSound[0])
		? LoadObject<USoundBase>(nullptr, Row->RandomSound) : nullptr;
	DeathSound = (Row->DeathSound && Row->DeathSound[0])
		? LoadObject<USoundBase>(nullptr, Row->DeathSound) : nullptr;

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
	NextAttackTime = Now + 0.5f;  // brief reaction beat before the first attack
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
		return;  // corpse: billboard only
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

	const float AttackRange = Row->bRangedAttack
		? FMath::Min(Row->AttackRangeCm, GRangedRangeCapCm)
		: Row->RadiusCm + GMeleeReachCm;
	const bool bCanAttack = Row->AttackDamage > 0.0f;  // civilians never attack

	if (bCanAttack && Dist <= AttackRange && Now >= NextAttackTime
		&& (bHasLOS || !Row->bRangedAttack))
	{
		NextAttackTime = Now + Row->AttackIntervalS;
		PerformAttack(Player);
	}

	// Direct Marathon steering: straight at the player on XY; when blocked
	// (chasing but barely moving), veer 45 degrees for half a second.
	if (Dist > AttackRange * 0.8f)
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

	// --- Walk animation: cycle the move frames while actually moving. ---
	if (MoveTextures.Num() > 0 && GetVelocity().SizeSquared2D() > 10.0f * 10.0f)
	{
		AnimTime += DeltaSeconds;
		const float FrameTime = FMath::Max(Row->MoveFrameTimeS, 0.033f);
		const int32 Frame = static_cast<int32>(AnimTime / FrameTime) % MoveTextures.Num();
		if (Frame != AnimFrame)
		{
			AnimFrame = Frame;
			SetSpriteTexture(MoveTextures[Frame]);
		}
	}
	else if (AnimFrame != -1)
	{
		AnimFrame = -1;
		SetSpriteTexture(StandTexture);
	}
}

void AM1Enemy::PerformAttack(APawn* Player)
{
	UWorld* World = GetWorld();
	if (!World || !Player || !Row)
	{
		return;
	}

	// Flash the stand pose for the attack beat (no dedicated attack frames in
	// v1); the walk cycle resumes on the next animated tick.
	AnimFrame = -1;
	SetSpriteTexture(StandTexture);

	if (!Row->bRangedAttack)
	{
		// Melee: the in-range check just happened in Tick; land the hit.
		UGameplayStatics::ApplyDamage(Player, Row->AttackDamage, GetController(), this, nullptr);
		return;
	}

	// Ranged: a plain AM1Projectile aimed at the player's center, no blast,
	// no impact sound (v1). Instigator = this enemy so it ignores its shooter.
	const FVector Muzzle = GetActorLocation()
		+ FVector(0.0f, 0.0f, GetSimpleCollisionHalfHeight() * 0.4f);
	const FVector Dir = (Player->GetActorLocation() - Muzzle).GetSafeNormal();
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
		Proj->Init(Row->RangedSpeedCmS, Row->AttackDamage, 0.0f, nullptr);
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
	SetSpriteTexture(DeadTexture);
}
