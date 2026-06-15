#pragma once

#include "CoreMinimal.h"
#include "M1DeathmatchGameMode.h"
#include "M1KOTHGameMode.generated.h"

/**
 * King of the Hill: score by TIME spent on "the hill" (the centroid of the map's
 * PlayerStarts, radius HillRadius). Kills don't score -- they only contest control.
 * Select: ...?game=/Script/M1UE5.M1KOTHGameMode?listen
 */
UCLASS()
class M1UE5_API AM1KOTHGameMode : public AM1DeathmatchGameMode
{
	GENERATED_BODY()

public:
	virtual FString GetModeName() const override { return TEXT("KING OF THE HILL"); }

protected:
	virtual void AwardKill(AController* Killer, AController* Victim) override {}  // hill time only
	virtual void ScoreTick(float DeltaSeconds) override;

private:
	FVector HillCenter = FVector::ZeroVector;
	bool bHillSet = false;
	float HillRadius = 500.0f;
	/** Fractional seconds-on-hill accumulator per controller (awards whole points). */
	UPROPERTY()
	TMap<TObjectPtr<AController>, float> HillTime;
};
