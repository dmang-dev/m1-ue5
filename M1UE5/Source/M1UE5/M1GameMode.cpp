#include "M1GameMode.h"
#include "M1Character.h"
#include "M1PlayerController.h"
#include "M1Enemy.h"
#include "M1Platform.h"
#include "M1Elevator.h"
#include "M1Panel.h"
#include "M1Light.h"
#include "M1PlayerState.h"
#include "M1RuntimeLevel.h"
#include "M1SaveGame.h"
#include "M1HUD.h"
#include "M1SpawnTable.h"
#include "M1PlatformTable.h"
#include "M1PanelTable.h"
#include "M1LightTable.h"
#include "M1LevelEnvTable.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

AM1GameMode::AM1GameMode()
{
	DefaultPawnClass = AM1Character::StaticClass();
	PlayerControllerClass = AM1PlayerController::StaticClass();
	PlayerStateClass = AM1PlayerState::StaticClass();  // per-player body color in co-op
	HUDClass = AM1HUD::StaticClass();  // weapon-in-hand sprite viewmodel
}

void AM1GameMode::BeginPlay()
{
	Super::BeginPlay();
	// Installer Phase 1: in runtime mode the persistent map (L_Runtime) is blank --
	// build the current level's geometry at runtime BEFORE spawning the table-driven
	// actors, so they (and the player's first-Tick placement) land on real geometry.
	if (!AM1Character::GRuntimeLevelName.IsEmpty())
	{
		BuildRuntimeGeometry();
	}
	SpawnMonsters();
	SpawnDoors();
	SpawnElevators();
	SpawnPanels();
	SpawnLights();
	PlayLevelMusic();
}

void AM1GameMode::BuildRuntimeGeometry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Fresh install / no Asset Setup run yet: there's no converted data to build from.
	// Surface a clear "run setup" message instead of an empty level.
	if (!AM1RuntimeLevel::IsDataAvailable())
	{
		UE_LOG(LogTemp, Warning, TEXT("[M1Runtime] no converted data found (run M1AssetSetup); showing data-missing screen"));
		if (AM1PlayerController* PC = Cast<AM1PlayerController>(World->GetFirstPlayerController()))
		{
			PC->ShowDataMissing();
		}
		return;
	}
	FString Stem = AM1Character::GRuntimeLevelName;  // "L_00_Arrival"
	Stem.RemoveFromStart(TEXT("L_"));                 // -> "00_Arrival"
	AM1RuntimeLevel* RL = World->SpawnActor<AM1RuntimeLevel>(
		AM1RuntimeLevel::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	if (RL)
	{
		RL->BuildFromStem(Stem);
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Runtime] GameMode built runtime level '%s'"), *Stem);
}

void AM1GameMode::SpawnLights()
{
	// Marathon lighting is now baked PER-SURFACE: scen_to_obj writes each surface's
	// LITE brightness into its emissive material (tex_C_S_b<pct>), so the world is
	// lit room-by-room with NO dynamic lights -- no hotspots, no black gaps, never
	// brighter than the texture. All we do here is lock exposure to MANUAL so eye-
	// adaptation can't re-normalise brightness and wash the baked mood back out.
	// (The old per-room point-light path -- AM1Light + dimmed directional/fill --
	// produced hotspots + black gaps and is retired; AM1Light/M1LightTable remain
	// for a future MPC-based switchable-light system.)
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		return;  // a PPV already present -> respect it
	}
	if (APostProcessVolume* PPV = World->SpawnActor<APostProcessVolume>(
			APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator))
	{
		PPV->bUnbound = true;
		FPostProcessSettings& S = PPV->Settings;
		S.bOverride_AutoExposureMethod = true;
		S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		S.AutoExposureApplyPhysicalCameraExposure = 0;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = 0.0f;  // manual EV; raise = brighter, lower = darker
	}
}

void AM1GameMode::PlayLevelMusic()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FString MapName = AM1Character::M1LevelKey(World);
	// The menu runs its own game mode; never lay a campaign track over it.
	if (MapName.Contains(TEXT("MainMenu")))
	{
		return;
	}

	// Canonical per-level music: each level's Minf carries a song_index (0-15)
	// into the 16-track set -- that IS Marathon's mapping. Test/experimental maps
	// (no env row) get no music.
	const FM1LevelEnv* Env = M1LevelEnvForMap(MapName);
	int32 Track = Env ? Env->SongIndex : -1;
	if (Track < 0)
	{
		return;
	}
	Track = ((Track % 16) + 16) % 16;
	const FString Path = FString::Printf(
		TEXT("/Game/Audio/Music/A_music_%02d.A_music_%02d"), Track, Track);
	USoundBase* Song = LoadObject<USoundBase>(nullptr, *Path);
	if (!Song)
	{
		UE_LOG(LogTemp, Warning, TEXT("M1GameMode: music track missing: %s"), *Path);
		return;
	}
	// 2D, looping (the wave asset is flagged looping). The component does not
	// persist across level transition, so the next level starts its own track.
	Music = UGameplayStatics::SpawnSound2D(World, Song, MusicVolume);
	UE_LOG(LogTemp, Log, TEXT("M1GameMode: music track %d (song_index) for %s"),
		Track, *MapName);
}

void AM1GameMode::SpawnPanels()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FString MapName = AM1Character::M1LevelKey(World);
	const TArrayView<const FM1Panel> Panels = M1PanelsForLevel(MapName);

	int32 Spawned = 0;
	for (const FM1Panel& P : Panels)
	{
		if (P.Kind < 0)
		{
			continue;  // sentinel (level has no panels)
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AM1Panel* Panel = World->SpawnActor<AM1Panel>(
			AM1Panel::StaticClass(), FVector(P.X, P.Y, P.Z), FRotator::ZeroRotator, Params);
		if (Panel)
		{
			Panel->InitFromPanel(P);
			++Spawned;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("M1GameMode: spawned %d/%d panels for %s"),
		Spawned, Panels.Num(), *MapName);
}

void AM1GameMode::SpawnDoors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FString MapName = AM1Character::M1LevelKey(World);
	const TArrayView<const FM1Door> Doors = M1DoorsForLevel(MapName);

	int32 Spawned = 0;
	for (const FM1Door& D : Doors)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AM1Platform* Door = World->SpawnActor<AM1Platform>(
			AM1Platform::StaticClass(), FVector(D.X, D.Y, D.Z), FRotator::ZeroRotator, Params);
		if (Door)
		{
			Door->InitFromDoor(D);
			++Spawned;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("M1GameMode: spawned %d/%d doors for %s"),
		Spawned, Doors.Num(), *MapName);
}

void AM1GameMode::SpawnElevators()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FString MapName = AM1Character::M1LevelKey(World);
	const TArrayView<const FM1Elevator> Elevs = M1ElevatorsForLevel(MapName);

	int32 Spawned = 0;
	for (const FM1Elevator& E : Elevs)
	{
		if (E.HighZ <= E.LowZ)
		{
			continue;  // sentinel (level has no elevators)
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AM1Elevator* Elev = World->SpawnActor<AM1Elevator>(
			AM1Elevator::StaticClass(), FVector(E.X, E.Y, 0.0f), FRotator::ZeroRotator, Params);
		if (Elev)
		{
			Elev->InitFromElevator(E);
			++Spawned;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("M1GameMode: spawned %d/%d elevators for %s"),
		Spawned, Elevs.Num(), *MapName);
}

void AM1GameMode::SpawnMonsters()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// GetMapName() is the short package name, PIE-prefixed in-editor
	// (UEDPIE_0_L_00_Arrival); M1SpawnsForLevel matches by contained stem.
	const FString MapName = AM1Character::M1LevelKey(World);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Save-anywhere restore: if a full-state load is pending for this level, spawn
	// the SAVED living enemies (with their saved health) instead of the level table
	// -- so the dead stay dead and the wounded stay wounded. The pawn applies the
	// player half of the restore + clears the pending flag (see AM1Character).
	if (const UM1SaveGame* Save = AM1Character::M1PendingFullFor(MapName))
	{
		int32 Restored = 0;
		for (const FM1SavedEnemy& S : Save->Enemies)
		{
			AM1Enemy* Enemy = World->SpawnActor<AM1Enemy>(
				AM1Enemy::StaticClass(), S.Location + FVector(0, 0, 8.0f), S.Rotation, Params);
			if (!Enemy)
			{
				continue;
			}
			if (!Enemy->InitFromType(S.MonsterType))
			{
				Enemy->Destroy();
				continue;
			}
			Enemy->M1SetHitPoints(S.HitPoints);
			++Restored;
		}
		UE_LOG(LogTemp, Log, TEXT("M1GameMode: RESTORED %d saved enemies for %s"),
			Restored, *MapName);
		return;
	}

	const TArrayView<const FM1MonsterSpawn> Spawns = M1SpawnsForLevel(MapName);
	int32 Spawned = 0;
	for (const FM1MonsterSpawn& S : Spawns)
	{
		// Placement Z is floor-relative (usually 0): start the capsule a little
		// above it; AM1Enemy::InitFromType snaps it down onto the sector mesh.
		const FVector Location(S.X, S.Y, S.Z + 60.0f);
		const FRotator Rotation(0.0f, S.FacingDeg, 0.0f);
		AM1Enemy* Enemy = World->SpawnActor<AM1Enemy>(
			AM1Enemy::StaticClass(), Location, Rotation, Params);
		if (!Enemy)
		{
			continue;
		}
		if (!Enemy->InitFromType(S.MonsterType))
		{
			Enemy->Destroy();  // type not in the generated table
			continue;
		}
		++Spawned;
	}

	UE_LOG(LogTemp, Log, TEXT("M1GameMode: spawned %d/%d monsters for %s"),
		Spawned, Spawns.Num(), *MapName);
}

AActor* AM1GameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Marathon start rooms are tight, so AGameModeBase's capsule-encroachment
	// check rejects our PlayerStart and the engine spawns at world origin (the
	// player drops into the void and falls forever). Just use the first
	// PlayerStart actor placed in the level.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			return *It;
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}
