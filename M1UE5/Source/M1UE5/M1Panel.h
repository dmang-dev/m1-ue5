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
	int32 Kind = 0;            // 0 = shield charger, 1 = pattern buffer, 2 = oxygen
	float Cap = 150.0f;        // charger fill target
	float UseRadius = 170.0f;
};
