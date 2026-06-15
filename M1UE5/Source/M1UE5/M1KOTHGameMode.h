#pragma once

#include "CoreMinimal.h"
#include "M1DeathmatchGameMode.h"
#include "M1KOTHGameMode.generated.h"

class AM1Hill;

/**
 * King of the Hill: score by TIME spent on "the hill". The hill is an authored
 * AM1Hill actor placed in the level (radius from the actor); if the map has none,
 * the mode spawns a visible one at the PlayerStarts centroid. Kills don't score --
 * they only contest control.
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
	/** Find the authored hill, or spawn one at the PlayerStarts centroid. */
	void EnsureHill();

	UPROPERTY()
	TObjectPtr<AM1Hill> Hill = nullptr;
	bool bHillResolved = false;

	/** Fractional seconds-on-hill accumulator per controller (awards whole points). */
	UPROPERTY()
	TMap<TObjectPtr<AController>, float> HillTime;
};
