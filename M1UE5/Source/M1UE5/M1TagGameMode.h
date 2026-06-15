#pragma once

#include "CoreMinimal.h"
#include "M1DeathmatchGameMode.h"
#include "M1TagGameMode.generated.h"

/**
 * Kill the Man with the Ball ("Tag"): one player is the carrier and scores while
 * holding the ball; you FRAG the carrier to take it (any kill makes the killer the
 * new carrier). Score = time as carrier.
 * Select: ...?game=/Script/M1UE5.M1TagGameMode?listen
 */
UCLASS()
class M1UE5_API AM1TagGameMode : public AM1DeathmatchGameMode
{
	GENERATED_BODY()

public:
	virtual FString GetModeName() const override { return TEXT("TAG"); }

protected:
	/** A kill hands the ball to the killer (you frag to steal it). */
	virtual void AwardKill(AController* Killer, AController* Victim) override;
	virtual void ScoreTick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<AController> Carrier = nullptr;
	float CarryTime = 0.0f;  // fractional seconds-as-carrier accumulator
};
