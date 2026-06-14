#include "M1GameMode.h"
#include "M1Character.h"
#include "M1PlayerController.h"
#include "M1Enemy.h"
#include "M1Platform.h"
#include "M1Elevator.h"
#include "M1Panel.h"
#include "M1Light.h"
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
	HUDClass = AM1HUD::StaticClass();  // weapon-in-hand sprite viewmodel
}

void AM1GameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnMonsters();
	SpawnDoors();
	SpawnElevators();
	SpawnPanels();
	SpawnLights();
	PlayLevelMusic();
}

void AM1GameMode::SpawnLights()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FString MapName = World->GetMapName();
	const TArrayView<const FM1Light> Lights = M1LightsForLevel(MapName);

	int32 Spawned = 0;
	for (const FM1Light& L : Lights)
	{
		if (L.LiteIndex < 0)
		{
			continue;  // sentinel (level has no lights)
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AM1Light* Light = World->SpawnActor<AM1Light>(
			AM1Light::StaticClass(), FVector(L.X, L.Y, L.Z), FRotator::ZeroRotator, Params);
		if (Light)
		{
			Light->InitFromLight(L);
			++Spawned;
		}
	}

	// With per-room lights present, knock the baked global key/fill way down so the
	// level reads room-by-room (Marathon mood) instead of a flat directional wash.
	// Done at runtime so no geometry rebake is needed to retune the ambient.
	if (Spawned > 0)
	{
		// Directional key, dimmed. (A SkyLight is near-useless here -- it captures the
		// surrounding scene, and enclosed Marathon levels have no sky, so its
		// intensity barely registers. Use directional fill for the ambient floor.)
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			if (ULightComponent* C = (*It)->GetLightComponent())
			{
				C->SetIntensity(0.50f);
			}
		}
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			if (USkyLightComponent* C = (*It)->GetLightComponent())
			{
				C->SetIntensity(0.30f);
			}
		}
		// Fill directional light from a different angle so surfaces facing away from
		// the key still get a navigable floor of light (unlit corridors weren't lit
		// by the per-room points alone -> near-black). Low intensity keeps mood.
		if (ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>(
				ADirectionalLight::StaticClass(),
				FVector(0, 0, 1500), FRotator(-30.0f, 210.0f, 0.0f)))
		{
			if (ULightComponent* C = Fill->GetLightComponent())
			{
				C->SetIntensity(0.40f);
				C->SetCastShadows(false);
			}
		}

		// Use MANUAL exposure (no eye-adaptation): auto-exposure was normalizing
		// brightness back up and erasing the per-room mood no matter how the lights
		// were dimmed. Manual + a fixed bias makes the light levels directly control
		// how bright the scene reads. Tunable at runtime -- no geometry rebake.
		bool bHasPPV = false;
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			bHasPPV = true;
			break;
		}
		if (!bHasPPV)
		{
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
				S.AutoExposureBias = 0.0f;  // manual EV; raise=brighter, lower=darker
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("M1GameMode: spawned %d/%d lights for %s"),
		Spawned, Lights.Num(), *MapName);
}

void AM1GameMode::PlayLevelMusic()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FString MapName = World->GetMapName();
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
	const FString MapName = World->GetMapName();
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
	const FString MapName = World->GetMapName();
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
	const FString MapName = World->GetMapName();
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
	const FString MapName = World->GetMapName();
	const TArrayView<const FM1MonsterSpawn> Spawns = M1SpawnsForLevel(MapName);

	int32 Spawned = 0;
	for (const FM1MonsterSpawn& S : Spawns)
	{
		// Placement Z is floor-relative (usually 0): start the capsule a little
		// above it; AM1Enemy::InitFromType snaps it down onto the sector mesh.
		const FVector Location(S.X, S.Y, S.Z + 60.0f);
		const FRotator Rotation(0.0f, S.FacingDeg, 0.0f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
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
