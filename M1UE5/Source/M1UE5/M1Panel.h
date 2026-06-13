#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M1PanelTable.h"
#include "M1Panel.generated.h"

class UStaticMeshComponent;
class USoundBase;
class AM1Character;

/**
 * A Marathon wall control panel: a shield recharger (refills to its cap on use)
 * or a pattern buffer (saves the campaign checkpoint on use). Spawned at level
 * load by AM1GameMode; activated through AM1Character::OnUse when the player is
 * close and facing it.
 */
UCLASS()
class M1UE5_API AM1Panel : public AActor
{
	GENERATED_BODY()

public:
	AM1Panel();

	void InitFromPanel(const FM1Panel& Panel);

	/** Do the panel's action for this player. Returns true if it fired. */
	bool Activate(AM1Character* Player);

	float GetUseRadius() const { return UseRadius; }

	/** Debug line for the test harness: kind + position. */
	FString M1DebugString() const;

private:
	UPROPERTY()
	UStaticMeshComponent* Mesh = nullptr;

	USoundBase* Sound = nullptr;
	int32 Kind = 0;            // 0=charger 1=buffer 2=oxygen 3=platform-switch 4=tag-switch
	float Cap = 150.0f;        // charger fill target
	int32 Target = -1;         // switch target: door polygon index (kind 3) / tag (kind 4)
	float UseRadius = 170.0f;

	/** Force-open every door this switch controls. Returns how many it opened. */
	int32 FireSwitch();
};
