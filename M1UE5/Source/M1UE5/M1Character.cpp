#include "M1Character.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "Engine/EngineTypes.h"
#include "SceneView.h"  // GNearClippingPlane
#include "Sound/SoundBase.h"
#include "Engine/DamageEvents.h"
#include "UObject/ConstructorHelpers.h"
#include "M1WeaponComponent.h"
#include "M1PlayerController.h"
#include "M1Tuning.h"
#include "M1TerminalTable.h"
#include "M1LevelEnvTable.h"
#include "M1SaveGame.h"
#include "M1Panel.h"
#include "M1Platform.h"
#include "M1Elevator.h"
#include "M1Light.h"
#include "M1Enemy.h"
#include "EngineUtils.h"  // TActorIterator
#include "TimerManager.h"  // SetTimer + FTimerDelegate (save-anywhere restore)
#include "Net/UnrealNetwork.h"  // DOREPLIFETIME (co-op replication)

namespace
{
	// Single save slot, shared by the checkpoint and the full save-anywhere snapshot.
	const TCHAR* const GM1SaveSlot = TEXT("M1Campaign");
}

// All 37 cooked Marathon levels (sorted), for in-game level cycling.
namespace
{
	static const TCHAR* const GM1Levels[] = {
		TEXT("/Game/Levels/M1/L_00_Arrival"),
		TEXT("/Game/Levels/M1/L_01_Bigger_Guns_Nearby"),
		TEXT("/Game/Levels/M1/L_02_Never_Burn_Money"),
		TEXT("/Game/Levels/M1/L_03_Defend_THIS"),
		TEXT("/Game/Levels/M1/L_04_Couch_Fishing"),
		TEXT("/Game/Levels/M1/L_05_The_Rose"),
		TEXT("/Game/Levels/M1/L_06_Smells_Like_Napalm_Tastes_Like_Chicken"),
		TEXT("/Game/Levels/M1/L_07_Cool_Fusion"),
		TEXT("/Game/Levels/M1/L_08_G4_Sunbathing"),
		TEXT("/Game/Levels/M1/L_09_Blaspheme_Quarantine"),
		TEXT("/Game/Levels/M1/L_10_Bob_B_Q"),
		TEXT("/Game/Levels/M1/L_11_Shake_Before_Using"),
		TEXT("/Game/Levels/M1/L_12_Fire_Fire_Fire_Fire_Fire"),
		TEXT("/Game/Levels/M1/L_13_Colony_Ship_For_Sale_Cheap"),
		TEXT("/Game/Levels/M1/L_14_Habe_Quiddam"),
		TEXT("/Game/Levels/M1/L_15_Neither_High_nor_Low"),
		TEXT("/Game/Levels/M1/L_16_Pfhor_Your_Eyes_Only"),
		TEXT("/Game/Levels/M1/L_17_No_Artificial_Colors"),
		TEXT("/Game/Levels/M1/L_18_Unpfhorgiven"),
		TEXT("/Game/Levels/M1/L_19_Two_Times_Two_Equals"),
		TEXT("/Game/Levels/M1/L_20_Beware_of_Low_Flying_Defense_Drones"),
		TEXT("/Game/Levels/M1/L_21_Eupfhoria"),
		TEXT("/Game/Levels/M1/L_22_Pfhoraphobia"),
		TEXT("/Game/Levels/M1/L_23_Ain_t_Got_Time_Pfhor_This"),
		TEXT("/Game/Levels/M1/L_24_Welcome_to_the_Revolution"),
		TEXT("/Game/Levels/M1/L_25_Try_again"),
		TEXT("/Game/Levels/M1/L_26_Ingue_Ferroque"),
		TEXT("/Game/Levels/M1/L_27_Mars_Needs_Women"),
		TEXT("/Game/Levels/M1/L_28_Carnage_Palace_Deeee_Luxe"),
		TEXT("/Game/Levels/M1/L_29_5_D_space"),
		TEXT("/Game/Levels/M1/L_30_Arena"),
		TEXT("/Game/Levels/M1/L_31_E_equals_MC_WHAT"),
		TEXT("/Game/Levels/M1/L_32_Showered_With_Grenades"),
		TEXT("/Game/Levels/M1/L_33_Spiral_Insanity"),
		TEXT("/Game/Levels/M1/L_34_Waldo_World_Arena"),
		TEXT("/Game/Levels/M1/L_35_What_Goes_Up_Must_Come_Down"),
		TEXT("/Game/Levels/M1/L_36_You_don_t_need_to_see_my_I_D"),
		// Debug terminal-look test rooms (L_TEST_*) and full-Arrival look variants
		// (L_ARR_*) are intentionally NOT shipped in the cycler or cooked into the
		// build. Their source OBJs are kept in Reference/Geometry for posterity;
		// rebuild any one for testing with e.g. M1_ONLY=ARR_Hybrid (or
		// M1_INCLUDE_TEST=1 to build them all) on ue5_headless_build.py.
	};
	static constexpr int32 GM1LevelCount = UE_ARRAY_COUNT(GM1Levels);
}

AM1Character::AM1Character()
{
	PrimaryActorTick.bCanEverTick = true;  // vacuum oxygen drain (no-op off-vacuum)

	// Mouse yaw turns the whole pawn; pitch is camera-only (typical FPS).
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// --- Networking (co-op / deathmatch). Replicate the pawn; ACharacter's
	//     CharacterMovementComponent then replicates movement automatically, and
	//     the Replicated Health/Oxygen below sync to clients. Single-player runs as
	//     Standalone (HasAuthority() always true), so this is inert there. ---
	bReplicates = true;
	SetReplicateMovement(true);

	// Marathon at 100cm/World-Unit has ~1m ceilings (tightest start room is 90cm),
	// so the player must be short — Marathon's own eye height is ~0.5 WU. A tall
	// default capsule pokes through every ceiling ("stuck in the ceiling") and
	// can't spawn in the start rooms. Size to fit a 90cm room and always spawn.
	GetCapsuleComponent()->InitCapsuleSize(22.0f, 32.0f);  // r=22, hh=32 (64cm tall)
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	// Eye ~50cm above the floor (capsule centre floor+32, +18) ~ Marathon's 0.5 WU.
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 18.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	// Marathon rendered a narrower FOV than UE's 90 default. The wide default
	// badly exaggerates perspective shear on off-axis surfaces (wall terminals
	// look "slanted" until you square up). 75 reads closer to the original and
	// keeps inset panels readable; tune to taste.
	FirstPersonCamera->SetFieldOfView(M1Tuning::GPlayerFOV);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->JumpZVelocity = M1Tuning::GJumpZVelocity;
		Move->AirControl = M1Tuning::GAirControl;
		Move->MaxWalkSpeed = M1Tuning::GWalkSpeed;
	}

	// Canonical Marathon teleport-in (sound code 1, 'snd ' resource id 2020),
	// played on level arrival so cycling levels with [ / ] reads as Marathon's
	// inter-level teleport. The "Weapon" folder is just py-marathon-utils'
	// cosmetic id//1000 bucket, not a real category. (Departure sound -- code 2,
	// resource 2010 / A_02010 -- is available if we later add a teleport-out.)
	static ConstructorHelpers::FObjectFinder<USoundBase> TeleportFinder(
		TEXT("/Game/Audio/Weapon/A_teleport_in.A_teleport_in"));
	if (TeleportFinder.Succeeded())
	{
		TeleportSound = TeleportFinder.Object;
	}

	Weapon = CreateDefaultSubobject<UM1WeaponComponent>(TEXT("Weapon"));

	// Co-op visible body: a placeholder box other players see. Hidden for the owning
	// (first-person) client via bOwnerNoSee; no collision (the capsule handles it).
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyCube(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (BodyCube.Succeeded())
	{
		Body->SetStaticMesh(BodyCube.Object);
	}
	Body->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.62f));  // ~capsule-sized
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetOwnerNoSee(true);   // owner is first-person; only teammates see it
	Body->SetCastShadow(false);

	// Renders the world into the bezel's 4:3 viewport hole. Attached to the eye so
	// it shares the player's view; the HUD composites its target into the frame.
	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("BezelCapture"));
	SceneCapture->SetupAttachment(FirstPersonCamera);
	SceneCapture->bCaptureEveryFrame = true;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->FOVAngle = M1Tuning::GPlayerFOV;  // match the camera (bezel hole aspect already fits)
	SceneCapture->bAlwaysPersistRenderingState = true;
}

void AM1Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AM1Character::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AM1Character::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("NextLevel", IE_Pressed, this, &AM1Character::NextLevel);
	PlayerInputComponent->BindAction("PrevLevel", IE_Pressed, this, &AM1Character::PrevLevel);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AM1Character::OnFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AM1Character::OnFireStop);
	PlayerInputComponent->BindAction("AltFire", IE_Pressed, this, &AM1Character::OnAltFire);
	PlayerInputComponent->BindAction("AltFire", IE_Released, this, &AM1Character::OnAltFireStop);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AM1Character::OnReload);
	PlayerInputComponent->BindAction("Use", IE_Pressed, this, &AM1Character::OnUse);

	// Weapon selection: number keys 1-7 -> slots 0-6, scroll wheel cycles.
	PlayerInputComponent->BindAction("Weapon1", IE_Pressed, this, &AM1Character::OnSelectWeapon1);
	PlayerInputComponent->BindAction("Weapon2", IE_Pressed, this, &AM1Character::OnSelectWeapon2);
	PlayerInputComponent->BindAction("Weapon3", IE_Pressed, this, &AM1Character::OnSelectWeapon3);
	PlayerInputComponent->BindAction("Weapon4", IE_Pressed, this, &AM1Character::OnSelectWeapon4);
	PlayerInputComponent->BindAction("Weapon5", IE_Pressed, this, &AM1Character::OnSelectWeapon5);
	PlayerInputComponent->BindAction("Weapon6", IE_Pressed, this, &AM1Character::OnSelectWeapon6);
	PlayerInputComponent->BindAction("Weapon7", IE_Pressed, this, &AM1Character::OnSelectWeapon7);
	PlayerInputComponent->BindAction("NextWeapon", IE_Pressed, this, &AM1Character::OnNextWeapon);
	PlayerInputComponent->BindAction("PrevWeapon", IE_Pressed, this, &AM1Character::OnPrevWeapon);
}

void AM1Character::OnFire()
{
	// Reading a terminal suppresses weapon fire (the gun isn't even drawn).
	if (!bTerminalOpen && Weapon)
	{
		Weapon->StartFire();
	}
}

void AM1Character::OnFireStop()
{
	if (Weapon)
	{
		Weapon->StopFire();
	}
}

void AM1Character::OnAltFire()
{
	if (!bTerminalOpen && Weapon)
	{
		Weapon->StartAltFire();
	}
}

void AM1Character::OnAltFireStop()
{
	if (Weapon)
	{
		Weapon->StopAltFire();
	}
}

void AM1Character::OnReload()
{
	if (Weapon)
	{
		Weapon->Reload();
	}
}

void AM1Character::OnUse()
{
	// Open -> page forward; past the last page -> close. Closed -> try to open
	// the terminal the player is standing at and facing.
	if (bTerminalOpen)
	{
		if (ActiveTermPage + 1 < ActiveTermPages)
		{
			++ActiveTermPage;
		}
		else
		{
			// Finishing the level's exit (main) terminal teleports onward, like
			// Marathon's control-room terminal. Capture before CloseTerminal()
			// clears ActiveTermId.
			const bool bExit = IsLevelExitTerminal(ActiveTermId);
			CloseTerminal();
			if (bExit)
			{
				AdvanceLevel();
			}
		}
		return;
	}
	// A wall panel (charger / pattern buffer) takes priority when you're right at
	// one; otherwise fall through to the terminal reader.
	if (UseNearestPanel())
	{
		return;
	}
	OpenNearestTerminal();
}

bool AM1Character::UseNearestPanel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const FVector Eye = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	AM1Panel* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<AM1Panel> It(World); It; ++It)
	{
		AM1Panel* Panel = *It;
		const FVector To = Panel->GetActorLocation() - Eye;
		const float DistSq = To.SizeSquared();
		const float R = Panel->GetUseRadius();
		if (DistSq > R * R)
		{
			continue;
		}
		// Must be roughly in front (don't trigger panels behind the player).
		if (FVector::DotProduct(Forward, To.GetSafeNormal2D()) < 0.3f)
		{
			continue;
		}
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Panel;
		}
	}
	return Best && Best->Activate(this);
}

void AM1Character::RefillShields(float Cap)
{
	Health = FMath::Max(Health, Cap);  // top up only -- never reduce an overcharge
	ShowShields();
}

void AM1Character::RefillOxygen(float Cap)
{
	// Top up toward Cap (clamped to the suit max); never reduce.
	Oxygen = FMath::Clamp(FMath::Max(Oxygen, Cap), 0.0f, M1Tuning::GPlayerOxygen);
}

void AM1Character::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Save-anywhere: apply the full-state snapshot on the first Tick (Tick doesn't
	// run while the chapter splash pauses the game, and by now the weapon's loadout
	// arrays exist). One-shot: clears both the per-pawn and the static pending flags.
	if (bPendingRestore)
	{
		bPendingRestore = false;
		GPendingFullLoad = false;
		if (UM1SaveGame* Save = Cast<UM1SaveGame>(UGameplayStatics::LoadGameFromSlot(GM1SaveSlot, 0)))
		{
			M1ApplyFullState(Save);
		}
	}

	// Oxygen only matters on vacuum levels, and not while a terminal is open
	// (movement is frozen there -- don't tick the suffocation clock).
	if (!bIsVacuumLevel || bTerminalOpen)
	{
		return;
	}
	if (Oxygen > 0.0f)
	{
		Oxygen = FMath::Max(0.0f, Oxygen - M1Tuning::GOxygenDrainPerSec * DeltaSeconds);
	}
	else
	{
		// Out of air: suffocate through the normal damage path (which handles the
		// restart-on-death). Small per-frame bite -> a steady drain on shields.
		UGameplayStatics::ApplyDamage(this, M1Tuning::GSuffocationDmgPerSec * DeltaSeconds,
			GetController(), this, nullptr);
	}
}

void AM1Character::OpenNearestTerminal()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Match the running map (possibly PIE-prefixed) to its terminal list.
	const TArrayView<const FM1Terminal> Terms = M1TerminalsForLevel(World->GetMapName());
	if (Terms.Num() == 0)
	{
		return;
	}

	// Use proximity (XY) within ~180cm AND "roughly in front" (camera forward
	// dotted with the direction to the panel is positive). Pick the closest.
	constexpr float kReachCm = 180.0f;
	const FVector Loc = GetActorLocation();
	const FVector Fwd = FirstPersonCamera
		? FirstPersonCamera->GetForwardVector()
		: GetActorForwardVector();

	const FM1Terminal* Best = nullptr;
	float BestDistSq = kReachCm * kReachCm;
	for (const FM1Terminal& T : Terms)
	{
		if (T.PageCount <= 0)
		{
			continue;
		}
		const FVector ToPanel(T.X - Loc.X, T.Y - Loc.Y, 0.0f);  // XY only
		const float DistSq = ToPanel.SizeSquared2D();
		if (DistSq > BestDistSq)
		{
			continue;
		}
		// Facing test in the XY plane (ignore pitch so looking slightly up/down
		// at a wall panel still counts).
		const FVector FwdFlat(Fwd.X, Fwd.Y, 0.0f);
		if (FVector::DotProduct(FwdFlat.GetSafeNormal(), ToPanel.GetSafeNormal()) <= 0.0f)
		{
			continue;
		}
		Best = &T;
		BestDistSq = DistSq;
	}

	if (!Best)
	{
		return;
	}

	bTerminalOpen = true;
	ActiveTermId = Best->TermId;
	ActiveTermPage = 0;
	ActiveTermPages = Best->PageCount;

	// Freeze the player: stop the pawn and disable movement input while reading.
	// (No canonical computer-interface logon sound is mapped in m1_sound_names.json,
	// so we skip the SFX rather than invent one.)
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
}

void AM1Character::CloseTerminal()
{
	bTerminalOpen = false;
	ActiveTermId = -1;
	ActiveTermPage = 0;
	ActiveTermPages = 0;

	// Restore walking.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}

void AM1Character::OnSelectWeapon1() { if (Weapon) { Weapon->SelectWeapon(0); } }
void AM1Character::OnSelectWeapon2() { if (Weapon) { Weapon->SelectWeapon(1); } }
void AM1Character::OnSelectWeapon3() { if (Weapon) { Weapon->SelectWeapon(2); } }
void AM1Character::OnSelectWeapon4() { if (Weapon) { Weapon->SelectWeapon(3); } }
void AM1Character::OnSelectWeapon5() { if (Weapon) { Weapon->SelectWeapon(4); } }
void AM1Character::OnSelectWeapon6() { if (Weapon) { Weapon->SelectWeapon(5); } }
void AM1Character::OnSelectWeapon7() { if (Weapon) { Weapon->SelectWeapon(6); } }

void AM1Character::OnNextWeapon()
{
	if (Weapon)
	{
		Weapon->NextWeapon();
	}
}

void AM1Character::OnPrevWeapon()
{
	if (Weapon)
	{
		Weapon->PrevWeapon();
	}
}

void AM1Character::BeginPlay()
{
	Super::BeginPlay();

	// Tight Marathon spaces (~1m rooms): a small near-clip stops the camera from
	// seeing through a wall when the player is pressed right against it.
	GNearClippingPlane = M1Tuning::GNearClipCm;

	// Re-assert the narrow FOV at runtime. Setting it only in the constructor does
	// NOT stick — the PlayerCameraManager's 90 default wins, which badly sheared
	// off-axis wall terminals. Applying it in BeginPlay makes it actually take.
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetFieldOfView(M1Tuning::GPlayerFOV);
	}

	// The L_MainMenu level runs the MENU game mode (spectator pawn). If it ever
	// falls back to the global gameplay game mode (the .umap GameMode override is
	// a regenerable artifact), this pawn can still spawn there -- so skip ALL the
	// gameplay presentation (teleport cue, HUD, debug stamps) on the menu so it
	// never draws the gun/shield-bar over the menu. AM1HUD::DrawHUD guards too.
	const bool bMenu = GetWorld() && GetWorld()->GetMapName().Contains(TEXT("MainMenu"));
	if (bMenu)
	{
		return;
	}

	// Full suit oxygen on level start. It only drains on vacuum levels (Minf env
	// bit 0 -- in M1, just L08 "G4 Sunbathing"); cache that here.
	Oxygen = M1Tuning::GPlayerOxygen;
	if (UWorld* W = GetWorld())
	{
		const FM1LevelEnv* Env = M1LevelEnvForMap(W->GetMapName());
		bIsVacuumLevel = Env && Env->bVacuum;
	}

	// Carry shields forward from the checkpoint. The checkpoint is stamped with
	// the level it belongs to (written on transition / New Game), so a stale save
	// from a different level never bleeds its shields into this one. Skipped when a
	// full save-anywhere load is pending -- that restores the exact saved shields.
	if (GetWorld() && !GPendingFullLoad)
	{
		int32 SavedIdx; float SavedShields;
		const int32 CurIdx = M1LevelIndexForMap(GetWorld()->GetMapName());
		if (CurIdx >= 0 && M1ReadCheckpoint(SavedIdx, SavedShields) && SavedIdx == CurIdx)
		{
			Health = FMath::Clamp(SavedShields, 1.0f, M1Tuning::GPlayerShields);
		}
	}

	// Save-anywhere restore: a full-state load is pending across the OpenLevel into
	// this (the saved) level. Apply the saved player state on a short timer so every
	// actor's BeginPlay (incl. the weapon component) has run; AM1GameMode separately
	// spawns the saved enemies. Clearing the flag here ends the pending load.
	// Apply on the first (unpaused) Tick rather than a timer: the level reload shows
	// the chapter splash which PAUSES game time (so a game-time timer never fires),
	// and Tick also guarantees the weapon component's BeginPlay has populated the
	// loadout arrays before we restore them.
	bPendingRestore = GPendingFullLoad;

	// Build the render target the HUD draws into the bezel's viewport hole. Hole
	// is ~450x273 px in the 640x480 bezel (aspect 1.648); 960x582 supersamples it.
	if (SceneCapture)
	{
		SceneRT = NewObject<UTextureRenderTarget2D>(this);
		SceneRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
		SceneRT->ClearColor = FLinearColor::Black;
		SceneRT->bAutoGenerateMips = false;
		SceneRT->InitAutoFormat(960, 582);
		SceneRT->UpdateResourceImmediate(true);
		SceneCapture->TextureTarget = SceneRT;
	}

	// Teleport-in SFX on level arrival (incl. [ / ] cycling). M1's atmosphere is
	// event-driven rather than an ambient bed, so this is the cue that fits the
	// current pre-combat state; monster/weapon/door SFX arrive with those systems.
	if (TeleportSound)
	{
		UGameplayStatics::SpawnSound2D(this, TeleportSound);
	}

	if (GEngine && GetWorld())
	{
		const FString MapName = GetWorld()->GetMapName();
		// Persistent build stamp (single shared value via GetBuildStamp) so the
		// running build is unmistakable in-game and matches the menu exactly.
		GEngine->AddOnScreenDebugMessage(0, 3600.0f, FColor::Yellow,
			FString::Printf(TEXT("M1UE5 BUILD  %s"), *GetBuildStamp()));
		GEngine->AddOnScreenDebugMessage(1, 8.0f, FColor::Green,
			FString::Printf(TEXT("Marathon UE5  -  %s"), *MapName));
		GEngine->AddOnScreenDebugMessage(2, 8.0f, FColor::Silver,
			TEXT("WASD move | mouse look | Space jump | [ and ] cycle levels"));
	}

	ShowShields();
}

float AM1Character::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (Health <= 0.0f || DamageAmount <= 0.0f)
	{
		return 0.0f;  // already dead (level restart pending) or a no-op hit
	}

	const float Applied = DamageAmount * M1Tuning::GIncomingDamageScale;
	Health -= Applied;
	ShowShields();

	if (Health <= 0.0f)
	{
		// Marathon restart-on-death v1: reload the current level (pattern
		// buffers / saves are a later system).
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::OpenLevel(this, FName(*World->GetName()));
		}
	}
	return Applied;
}

void AM1Character::ShowShields() const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(11, 3600.0f, FColor::Cyan,
			FString::Printf(TEXT("Shields  %d"), FMath::Max(0, FMath::CeilToInt(Health))));
	}
}

float AM1Character::GetMaxHealth() const
{
	// 1x shields. The HUD bar fills from this; raising GPlayerShields raises both
	// the start value (BeginPlay) and the bar's full mark together.
	return M1Tuning::GPlayerShields;
}

float AM1Character::GetMaxOxygen() const
{
	return M1Tuning::GPlayerOxygen;
}

void AM1Character::M1SetOxygen(float NewOxygen)
{
	Oxygen = FMath::Clamp(NewOxygen, 0.0f, M1Tuning::GPlayerOxygen);
	UE_LOG(LogTemp, Log, TEXT("M1SetOxygen -> %.0f / %.0f"), Oxygen, M1Tuning::GPlayerOxygen);
}

void AM1Character::M1Where()
{
	UWorld* W = GetWorld();
	const FVector L = GetActorLocation();
	const FRotator R = GetActorRotation();
	UE_LOG(LogTemp, Warning,
		TEXT("[M1Where] map=%s pos=(%.0f,%.0f,%.0f) yaw=%.0f shields=%.0f/%.0f oxygen=%.0f/%.0f vacuum=%d"),
		W ? *W->GetMapName() : TEXT("?"), L.X, L.Y, L.Z, R.Yaw,
		Health, GetMaxHealth(), Oxygen, GetMaxOxygen(), bIsVacuumLevel ? 1 : 0);
}

void AM1Character::M1Doors()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	int32 i = 0;
	for (TActorIterator<AM1Platform> It(W); It; ++It)
	{
		UE_LOG(LogTemp, Warning, TEXT("[M1Doors] #%d %s"), i++, *(*It)->M1DebugString());
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Doors] total %d"), i);
}

void AM1Character::M1Elevators()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	int32 i = 0;
	for (TActorIterator<AM1Elevator> It(W); It; ++It)
	{
		UE_LOG(LogTemp, Warning, TEXT("[M1Elevators] #%d %s"), i++, *(*It)->M1DebugString());
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Elevators] total %d"), i);
}

void AM1Character::M1Panels()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	int32 i = 0;
	for (TActorIterator<AM1Panel> It(W); It; ++It)
	{
		UE_LOG(LogTemp, Warning, TEXT("[M1Panels] #%d %s"), i++, *(*It)->M1DebugString());
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Panels] total %d"), i);
}

void AM1Character::M1Lights()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	int32 i = 0, on = 0;
	for (TActorIterator<AM1Light> It(W); It; ++It)
	{
		if ((*It)->IsOn()) { ++on; }
		UE_LOG(LogTemp, Warning, TEXT("[M1Lights] #%d %s"), i++, *(*It)->M1DebugString());
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Lights] total %d (on %d)"), i, on);
}

void AM1Character::M1Radar()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	const float RangeCm = 5000.0f;
	const FVector P = GetActorLocation();
	int32 Total = 0, Active = 0, Friendly = 0;
	for (TActorIterator<AM1Enemy> It(W); It; ++It)
	{
		AM1Enemy* E = *It;
		if (!E || !E->IsAlive()) { continue; }
		FVector D = E->GetActorLocation() - P;
		D.Z = 0.0f;
		if (D.SizeSquared() > RangeCm * RangeCm) { continue; }
		++Total;
		if (E->IsActive()) { ++Active; }
		if (E->IsFriendly()) { ++Friendly; }
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Radar] within %.0fcm: %d blips (%d active, %d friendly)"),
		RangeCm, Total, Active, Friendly);
}

void AM1Character::M1Net()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	const TCHAR* Mode =
		W->GetNetMode() == NM_Standalone ? TEXT("Standalone") :
		W->GetNetMode() == NM_ListenServer ? TEXT("ListenServer") :
		W->GetNetMode() == NM_DedicatedServer ? TEXT("DedicatedServer") :
		W->GetNetMode() == NM_Client ? TEXT("Client") : TEXT("?");
	int32 Players = 0, Enemies = 0;
	for (TActorIterator<AM1Character> It(W); It; ++It) { ++Players; }
	for (TActorIterator<AM1Enemy> It(W); It; ++It) { ++Enemies; }
	UE_LOG(LogTemp, Warning, TEXT("[M1Net] mode=%s authority=%d players=%d enemies=%d"),
		Mode, HasAuthority() ? 1 : 0, Players, Enemies);
}

void AM1Character::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AM1Character, Health);
	DOREPLIFETIME(AM1Character, Oxygen);
}

void AM1Character::M1Terms()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	const TArrayView<const FM1Terminal> Terms = M1TerminalsForLevel(W->GetMapName());
	for (int32 i = 0; i < Terms.Num(); ++i)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[M1Terms] #%d id=%d pages=%d bExit=%d isExit=%d pos=(%.0f,%.0f,%.0f)"),
			i, Terms[i].TermId, Terms[i].PageCount, Terms[i].bExit ? 1 : 0,
			IsLevelExitTerminal(Terms[i].TermId) ? 1 : 0, Terms[i].X, Terms[i].Y, Terms[i].Z);
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Terms] total %d"), Terms.Num());
}

void AM1Character::M1Goto(float X, float Y, float Z, float Yaw)
{
	SetActorLocation(FVector(X, Y, Z), false, nullptr, ETeleportType::TeleportPhysics);
	if (Yaw < 999.0f)
	{
		if (AController* C = GetController())
		{
			C->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Goto] -> (%.0f,%.0f,%.0f) yaw=%.0f"), X, Y, Z, Yaw);
}

void AM1Character::M1Activate(int32 PanelIndex)
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	int32 i = 0;
	for (TActorIterator<AM1Panel> It(W); It; ++It)
	{
		if (i++ == PanelIndex)
		{
			const bool bFired = (*It)->Activate(this);
			UE_LOG(LogTemp, Warning, TEXT("[M1Activate] #%d %s -> %d"),
				PanelIndex, *(*It)->M1DebugString(), bFired ? 1 : 0);
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Activate] no panel #%d (have %d)"), PanelIndex, i);
}

void AM1Character::M1OpenTerm(int32 TermIndex)
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	const TArrayView<const FM1Terminal> Terms = M1TerminalsForLevel(W->GetMapName());
	if (TermIndex < 0 || TermIndex >= Terms.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[M1OpenTerm] no term #%d (have %d)"), TermIndex, Terms.Num());
		return;
	}
	const FM1Terminal& T = Terms[TermIndex];
	bTerminalOpen = true;
	ActiveTermId = T.TermId;
	ActiveTermPage = 0;
	ActiveTermPages = FMath::Max(1, T.PageCount);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1OpenTerm] #%d id=%d pages=%d isExit=%d (M1Use to page)"),
		TermIndex, T.TermId, ActiveTermPages, IsLevelExitTerminal(T.TermId) ? 1 : 0);
}

void AM1Character::M1Use()
{
	UE_LOG(LogTemp, Warning, TEXT("[M1Use] termOpen=%d page=%d/%d"),
		bTerminalOpen ? 1 : 0, ActiveTermPage, ActiveTermPages);
	OnUse();
}

void AM1Character::M1SelfTest()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const FString Map = W->GetMapName();
	int32 nMon = 0, nDoor = 0, nPanel = 0, nElev = 0, nSwitch = 0;
	for (TActorIterator<AM1Enemy> It(W); It; ++It) { ++nMon; }
	for (TActorIterator<AM1Platform> It(W); It; ++It) { ++nDoor; }
	for (TActorIterator<AM1Elevator> It(W); It; ++It) { ++nElev; }
	for (TActorIterator<AM1Panel> It(W); It; ++It) { ++nPanel; }
	const TArrayView<const FM1Terminal> Terms = M1TerminalsForLevel(Map);
	int32 ExitId = -1;
	for (const FM1Terminal& T : Terms)
	{
		if (IsLevelExitTerminal(T.TermId)) { ExitId = T.TermId; break; }
	}
	const int32 LvlIdx = M1LevelIndexForMap(Map);
	const FVector P = GetActorLocation();
	const bool bHasWeapon = Weapon != nullptr;

	// PASS criteria: a campaign level should have a pawn placed on real ground
	// (finite Z), a weapon, and an identifiable exit terminal when it has any.
	const bool bGrounded = FMath::Abs(P.Z) < 100000.0f;
	const bool bExitOk = (Terms.Num() == 0) || (ExitId >= 0);
	const bool bPass = bGrounded && bHasWeapon && bExitOk && LvlIdx >= 0;

	UE_LOG(LogTemp, Warning,
		TEXT("[M1SELFTEST] %s idx=%d pos=(%.0f,%.0f,%.0f) shields=%.0f oxy=%.0f vac=%d | "
		     "monsters=%d doors=%d elevators=%d panels=%d terminals=%d exitTerm=%d weapon=%d | %s"),
		*Map, LvlIdx, P.X, P.Y, P.Z, Health, Oxygen, bIsVacuumLevel ? 1 : 0,
		nMon, nDoor, nElev, nPanel, Terms.Num(), ExitId, bHasWeapon ? 1 : 0,
		bPass ? TEXT("PASS") : TEXT("FAIL"));
}

FString AM1Character::GetBuildStamp()
{
	// __DATE__/__TIME__ are this translation unit's compile time. Routing the menu
	// through this one function makes the menu + HUD stamps identical.
	return FString(TEXT(__DATE__)) + TEXT("  ") + TEXT(__TIME__);
}

void AM1Character::NextLevel()
{
	CycleLevel(1);
}

void AM1Character::PrevLevel()
{
	CycleLevel(-1);
}

void AM1Character::CycleLevel(int32 Direction)
{
	if (!GetWorld())
	{
		return;
	}
	const int32 Index = FMath::Max(0, M1LevelIndexForMap(GetWorld()->GetMapName()));
	const int32 Next = ((Index + Direction) % GM1LevelCount + GM1LevelCount) % GM1LevelCount;
	// Checkpoint the destination + carry current shields forward (Marathon
	// shields never regenerate, so the next level starts where this one left off).
	M1WriteCheckpoint(Next, Health);
	UGameplayStatics::OpenLevel(this, FName(M1LevelPath(Next)));
}

void AM1Character::AdvanceLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const int32 Index = M1LevelIndexForMap(World->GetMapName());
	if (Index < 0)
	{
		return;  // test / experimental map, not part of the campaign order
	}
	const int32 Next = Index + 1;
	if (Next >= GM1LevelCount)
	{
		// Campaign complete: show the epilogue (click returns to the main menu).
		UE_LOG(LogTemp, Log, TEXT("AdvanceLevel: campaign complete (final level %d)"), Index);
		if (AM1PlayerController* PC = Cast<AM1PlayerController>(GetController()))
		{
			PC->ShowEpilogue();
		}
		return;
	}
	M1WriteCheckpoint(Next, Health);  // carry shields forward (no regen)
	UE_LOG(LogTemp, Log, TEXT("AdvanceLevel: %d -> %d via exit terminal"), Index, Next);
	UGameplayStatics::OpenLevel(this, FName(M1LevelPath(Next)));
}

bool AM1Character::IsLevelExitTerminal(int32 TermId) const
{
	UWorld* World = GetWorld();
	if (!World || TermId < 0)
	{
		return false;
	}
	// M1 carries no teleport directive in the data, so identify the exit from the
	// generated bExit flag (terminals whose text shows a teleport action). Among
	// flagged terminals -- or all of them, when a level has none flagged -- the
	// most-paged one is the exit (the level's main/control-room terminal).
	const TArrayView<const FM1Terminal> Terms = M1TerminalsForLevel(World->GetMapName());
	bool bAnyExit = false;
	for (const FM1Terminal& T : Terms)
	{
		if (T.bExit) { bAnyExit = true; break; }
	}
	int32 BestPages = -1;
	int32 BestId = -1;
	for (const FM1Terminal& T : Terms)
	{
		if (bAnyExit && !T.bExit)
		{
			continue;  // a flagged exit exists -> only consider flagged terminals
		}
		if (T.PageCount > BestPages)
		{
			BestPages = T.PageCount;
			BestId = T.TermId;
		}
	}
	return BestId >= 0 && TermId == BestId;
}

int32 AM1Character::M1LevelCount()
{
	return GM1LevelCount;
}

const TCHAR* AM1Character::M1LevelPath(int32 Index)
{
	return GM1Levels[FMath::Clamp(Index, 0, GM1LevelCount - 1)];
}

int32 AM1Character::M1LevelIndexForMap(const FString& MapName)
{
	// GetMapName() is the short package name (possibly PIE-prefixed); match it
	// against each level's clean filename.
	for (int32 i = 0; i < GM1LevelCount; ++i)
	{
		if (MapName.EndsWith(FPaths::GetCleanFilename(GM1Levels[i])))
		{
			return i;
		}
	}
	return -1;
}

bool AM1Character::GPendingFullLoad = false;

void AM1Character::M1WriteCheckpoint(int32 LevelIndex, float Shields)
{
	UM1SaveGame* Save = Cast<UM1SaveGame>(
		UGameplayStatics::CreateSaveGameObject(UM1SaveGame::StaticClass()));
	if (!Save)
	{
		return;
	}
	Save->LevelIndex = LevelIndex;
	Save->Shields = Shields;
	Save->bValid = true;
	UGameplayStatics::SaveGameToSlot(Save, GM1SaveSlot, 0);
}

bool AM1Character::M1ReadCheckpoint(int32& OutLevelIndex, float& OutShields)
{
	if (!UGameplayStatics::DoesSaveGameExist(GM1SaveSlot, 0))
	{
		return false;
	}
	UM1SaveGame* Save = Cast<UM1SaveGame>(UGameplayStatics::LoadGameFromSlot(GM1SaveSlot, 0));
	if (!Save || !Save->bValid)
	{
		return false;
	}
	OutLevelIndex = Save->LevelIndex;
	OutShields = Save->Shields;
	return true;
}

bool AM1Character::M1HasCheckpoint()
{
	int32 Idx; float Sh;
	return M1ReadCheckpoint(Idx, Sh);
}

void AM1Character::M1SaveFull()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	UM1SaveGame* Save = Cast<UM1SaveGame>(
		UGameplayStatics::CreateSaveGameObject(UM1SaveGame::StaticClass()));
	if (!Save)
	{
		return;
	}
	// Keep the checkpoint fields valid so menu Continue still resumes the level.
	Save->LevelIndex = FMath::Max(0, M1LevelIndexForMap(W->GetMapName()));
	Save->Shields = Health;
	Save->bValid = true;
	// Full save-anywhere snapshot.
	Save->bFullState = true;
	Save->PlayerLocation = GetActorLocation();
	Save->PlayerRotation = GetController() ? GetController()->GetControlRotation() : GetActorRotation();
	Save->Health = Health;
	Save->Oxygen = Oxygen;
	if (Weapon)
	{
		Save->CurrentWeapon = Weapon->GetCurrentIndex();
		Save->Ammo = Weapon->GetAmmoArray();
		Save->SecAmmo = Weapon->GetSecAmmoArray();
	}
	for (TActorIterator<AM1Enemy> It(W); It; ++It)
	{
		AM1Enemy* E = *It;
		if (!E || !E->IsAlive() || E->GetMonsterType() < 0)
		{
			continue;  // only living enemies are saved -> the dead stay dead
		}
		FM1SavedEnemy S;
		S.MonsterType = E->GetMonsterType();
		S.Location = E->GetActorLocation();
		S.Rotation = E->GetActorRotation();
		S.HitPoints = E->GetHitPoints();
		Save->Enemies.Add(S);
	}
	UGameplayStatics::SaveGameToSlot(Save, GM1SaveSlot, 0);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(21, 2.5f, FColor::Green,
			FString::Printf(TEXT("Game saved (%d enemies)"), Save->Enemies.Num()));
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Save] %s: enemies=%d shields=%.0f oxy=%.0f wpn=%d"),
		*W->GetMapName(), Save->Enemies.Num(), Health, Oxygen, Save->CurrentWeapon);
}

void AM1Character::M1LoadFull()
{
	if (!UGameplayStatics::DoesSaveGameExist(GM1SaveSlot, 0))
	{
		return;
	}
	UM1SaveGame* Save = Cast<UM1SaveGame>(UGameplayStatics::LoadGameFromSlot(GM1SaveSlot, 0));
	if (!Save || !Save->bFullState)
	{
		return;  // no full snapshot -> nothing to restore
	}
	GPendingFullLoad = true;  // survives the OpenLevel; consumed on the next BeginPlay
	UE_LOG(LogTemp, Warning, TEXT("[M1Load] restoring level %d (%d enemies)"),
		Save->LevelIndex, Save->Enemies.Num());
	UGameplayStatics::OpenLevel(this, FName(M1LevelPath(Save->LevelIndex)));
}

UM1SaveGame* AM1Character::M1PendingFullFor(const FString& MapName)
{
	if (!GPendingFullLoad || !UGameplayStatics::DoesSaveGameExist(GM1SaveSlot, 0))
	{
		return nullptr;
	}
	UM1SaveGame* Save = Cast<UM1SaveGame>(UGameplayStatics::LoadGameFromSlot(GM1SaveSlot, 0));
	if (!Save || !Save->bFullState || M1LevelIndexForMap(MapName) != Save->LevelIndex)
	{
		return nullptr;
	}
	return Save;  // caller does NOT clear GPendingFullLoad -- the pawn clears it
}

void AM1Character::M1ApplyFullState(const UM1SaveGame* Save)
{
	if (!Save)
	{
		return;
	}
	SetActorLocation(Save->PlayerLocation, false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* C = GetController())
	{
		C->SetControlRotation(Save->PlayerRotation);
	}
	Health = FMath::Clamp(Save->Health, 1.0f, M1Tuning::GPlayerShields * 3.0f);
	Oxygen = FMath::Clamp(Save->Oxygen, 0.0f, M1Tuning::GPlayerOxygen);
	if (Weapon)
	{
		Weapon->RestoreLoadout(Save->CurrentWeapon, Save->Ammo, Save->SecAmmo);
	}
	ShowShields();
	UE_LOG(LogTemp, Warning, TEXT("[M1Load] applied: pos=(%.0f,%.0f,%.0f) shields=%.0f oxy=%.0f"),
		Save->PlayerLocation.X, Save->PlayerLocation.Y, Save->PlayerLocation.Z, Health, Oxygen);
}

void AM1Character::M1Save() { M1SaveFull(); }
void AM1Character::M1Load() { M1LoadFull(); }

void AM1Character::MoveForward(float Value)
{
	if (!bTerminalOpen && Controller && Value != 0.0f)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
	}
}

void AM1Character::MoveRight(float Value)
{
	if (!bTerminalOpen && Controller && Value != 0.0f)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
	}
}
