#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "M1GameMode.generated.h"

class UAudioComponent;

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
	/** Installer Phase 1: build the current runtime level's geometry (AM1RuntimeLevel)
	 *  into the blank persistent map. Only called when AM1Character::GRuntimeLevelName
	 *  is set (runtime mode). */
	void BuildRuntimeGeometry();

	/** Spawn every monster placement for the current map (M1SpawnTable.h). */
	void SpawnMonsters();

	/** Spawn the current map's door panels (M1PlatformTable.h). */
	void SpawnDoors();

	/** Spawn the current map's ride-on elevators (M1PlatformTable.h). */
	void SpawnElevators();

	/** Spawn the current map's wall control panels (M1PanelTable.h). */
	void SpawnPanels();

	/** Spawn the current map's per-room LITE lights (M1LightTable.h) and, when any
	 *  exist, dim the level's baked global DirectionalLight/SkyLight so the per-room
	 *  point lights provide the mood instead of a flat key wash. */
	void SpawnLights();

	/** Pick a music track by campaign level index and play it looping (2D).
	 *  M1 has no canonical per-level music, so this maps level i -> track i%16
	 *  deterministically; non-campaign/test maps fall back to track 0. */
	void PlayLevelMusic();

	/** The looping level-music component (held for GC; the world destroys it on
	 *  level transition, which stops the track so the next level starts fresh). */
	UPROPERTY()
	UAudioComponent* Music = nullptr;

	/** Music bed volume, below the SFX so cues stay legible. */
	UPROPERTY(EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MusicVolume = 0.55f;
};
