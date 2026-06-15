#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "M1SettingsSave.generated.h"

/**
 * Player preferences, persisted to the "M1Settings" slot (separate from the
 * campaign checkpoint / full-state save). Loaded once at pawn BeginPlay into the
 * AM1Character static settings and applied to look input. Kept deliberately small.
 */
UCLASS()
class M1UE5_API UM1SettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	/** Look-input multiplier (1.0 = default). */
	UPROPERTY()
	float MouseSensitivity = 1.0f;

	/** Invert the vertical look axis. */
	UPROPERTY()
	bool bInvertLook = false;
};
