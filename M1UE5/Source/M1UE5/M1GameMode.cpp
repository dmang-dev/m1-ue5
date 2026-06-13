#include "M1GameMode.h"
#include "M1Character.h"
#include "M1PlayerController.h"
#include "M1Enemy.h"
#include "M1Platform.h"
#include "M1Panel.h"
#include "M1HUD.h"
#include "M1SpawnTable.h"
#include "M1PlatformTable.h"
#include "M1PanelTable.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
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
	SpawnPanels();
	PlayLevelMusic();
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

	// Deterministic level->track map (no canonical M1 mapping exists). Campaign
	// maps use their story index; test/experimental maps (index -1) get track 0.
	int32 Idx = AM1Character::M1LevelIndexForMap(MapName);
	if (Idx < 0)
	{
		Idx = 0;
	}
	const int32 Track = ((Idx % 16) + 16) % 16;
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
	UE_LOG(LogTemp, Log, TEXT("M1GameMode: music track %d (level idx %d) for %s"),
		Track, Idx, *MapName);
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
