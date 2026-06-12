#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "M1GameMode.generated.h"

/** Default GameMode: spawns the first-person walking pawn at the PlayerStart
 *  and populates the level's monsters from the generated spawn table. */
UCLASS()
class M1UE5_API AM1GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AM1GameMode();

	virtual void BeginPlay() override;

	/** Use the level's PlayerStart unconditionally (the default rating rejects
	 *  Marathon's tight spawn rooms and the engine falls back to world origin). */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
	/** Spawn every monster placement for the current map (M1SpawnTable.h). */
	void SpawnMonsters();

	/** Spawn the current map's door panels (M1PlatformTable.h). */
	void SpawnDoors();
};
