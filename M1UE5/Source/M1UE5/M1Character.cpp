#include "M1Character.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
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
#include "M1Tuning.h"
#include "M1TerminalTable.h"
#include "M1SaveGame.h"

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
	PrimaryActorTick.bCanEverTick = false;

	// Mouse yaw turns the whole pawn; pitch is camera-only (typical FPS).
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

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
	FirstPersonCamera->SetFieldOfView(75.0f);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->JumpZVelocity = 420.0f;
		Move->AirControl = 0.5f;
		Move->MaxWalkSpeed = 600.0f;
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
			CloseTerminal();
		}
		return;
	}
	OpenNearestTerminal();
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
	GNearClippingPlane = 5.0f;

	// Re-assert the narrow FOV at runtime. Setting it only in the constructor does
	// NOT stick — the PlayerCameraManager's 90 default wins, which badly sheared
	// off-axis wall terminals. Applying it in BeginPlay makes it actually take.
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetFieldOfView(75.0f);
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

	// Carry shields forward from the checkpoint. The checkpoint is stamped with
	// the level it belongs to (written on transition / New Game), so a stale save
	// from a different level never bleeds its shields into this one.
	if (GetWorld())
	{
		int32 SavedIdx; float SavedShields;
		const int32 CurIdx = M1LevelIndexForMap(GetWorld()->GetMapName());
		if (CurIdx >= 0 && M1ReadCheckpoint(SavedIdx, SavedShields) && SavedIdx == CurIdx)
		{
			Health = FMath::Clamp(SavedShields, 1.0f, M1Tuning::GPlayerShields);
		}
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

namespace
{
	const TCHAR* const GM1SaveSlot = TEXT("M1Campaign");
}

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
