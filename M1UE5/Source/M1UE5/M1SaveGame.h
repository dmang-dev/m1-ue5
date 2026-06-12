#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "M1SaveGame.generated.h"

/**
 * Campaign checkpoint. Written on every level transition (and reset by New
 * Game); read on level entry to carry shields forward (Marathon shields never
 * regenerate) and by the menu's Continue to resume the right level.
 */
UCLASS()
class M1UE5_API UM1SaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 LevelIndex = 0;

	UPROPERTY()
	float Shields = 150.0f;

	UPROPERTY()
	bool bValid = false;
};
