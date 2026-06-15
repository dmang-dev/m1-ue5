#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "M1PlayerState.generated.h"

/**
 * Adds a replicated team number to the stock PlayerState (which already replicates
 * Score). TeamNum -1 = free-for-all (no team). Drives team friendly-fire and the
 * per-player / per-team body color so co-op and deathmatch players are
 * distinguishable on every machine.
 */
UCLASS()
class M1UE5_API AM1PlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UPROPERTY(Replicated)
	int32 TeamNum = -1;   // -1 = FFA; 0..3 = team

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	/** Body color: a fixed team color (0 red / 1 blue / 2 green / 3 gold), or a
	 *  per-player hue from the player id when free-for-all. */
	FLinearColor GetPlayerColor() const;
};
