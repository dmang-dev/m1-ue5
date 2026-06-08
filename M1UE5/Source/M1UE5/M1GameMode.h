#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "M1GameMode.generated.h"

/** Default GameMode: spawns the first-person walking pawn at the PlayerStart. */
UCLASS()
class M1UE5_API AM1GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AM1GameMode();

	/** Use the level's PlayerStart unconditionally (the default rating rejects
	 *  Marathon's tight spawn rooms and the engine falls back to world origin). */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
