#pragma once

#include "CoreMinimal.h"
#include "M1DeathmatchGameMode.h"
#include "M1TeamGameMode.generated.h"

/**
 * Team deathmatch: two teams (red/blue), round-robin assignment, friendly fire off,
 * bodies colored by team. Frag scoring as Carnage (per-player; team totals show on
 * the scoreboard by color). Select: ...?game=/Script/M1UE5.M1TeamGameMode?listen
 */
UCLASS()
class M1UE5_API AM1TeamGameMode : public AM1DeathmatchGameMode
{
	GENERATED_BODY()

public:
	AM1TeamGameMode()
	{
		NumTeams = 2;
	}

	virtual FString GetModeName() const override { return TEXT("TEAM CARNAGE"); }
};
