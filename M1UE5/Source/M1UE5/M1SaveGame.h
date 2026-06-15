#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "M1SaveGame.generated.h"

/** One saved monster, for save-anywhere (only LIVING enemies are stored, so the
 *  dead stay dead on restore -- no refighting). Re-spawned from Type on load. */
USTRUCT()
struct FM1SavedEnemy
{
	GENERATED_BODY()

	UPROPERTY() int32 MonsterType = -1;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() float HitPoints = 1.0f;
};

/**
 * Marathon save game. Two modes share one slot:
 *  - CHECKPOINT (bValid): written on level transition -- level index + shields,
 *    so Continue resumes the right level carrying shields forward.
 *  - FULL STATE (bFullState): a pattern-buffer / menu "save anywhere" snapshot --
 *    player pose + vitals + loadout and every living enemy, restored on load.
 */
UCLASS()
class M1UE5_API UM1SaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// --- Checkpoint (level-transition carry-over) ---
	UPROPERTY() int32 LevelIndex = 0;
	UPROPERTY() float Shields = 150.0f;
	UPROPERTY() bool bValid = false;

	// --- Full state (save-anywhere) ---
	UPROPERTY() bool bFullState = false;
	UPROPERTY() FVector PlayerLocation = FVector::ZeroVector;
	UPROPERTY() FRotator PlayerRotation = FRotator::ZeroRotator;
	UPROPERTY() float Health = 150.0f;
	UPROPERTY() float Oxygen = 100.0f;
	UPROPERTY() int32 CurrentWeapon = 0;
	UPROPERTY() TArray<int32> Ammo;       // per-weapon primary
	UPROPERTY() TArray<int32> SecAmmo;    // per-weapon secondary
	UPROPERTY() TArray<FM1SavedEnemy> Enemies;
};
