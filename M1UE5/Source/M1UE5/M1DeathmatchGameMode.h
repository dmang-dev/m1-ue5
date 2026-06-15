#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "M1DeathmatchGameMode.generated.h"

class AM1Character;

/**
 * Marathon net deathmatch -- "Carnage" / Every Man for Himself: free-for-all frag
 * scoring with instant respawn. Select it per-map via the URL game option, e.g.
 *   M1UE5.exe /Game/Levels/M1/L_30_Arena?game=/Script/M1UE5.M1DeathmatchGameMode?listen
 * Players are server-authoritative (see AM1Character) and shootable (capsule blocks
 * the weapon's Visibility trace), so PvP damage flows through the normal damage path.
 * KOTH and Kill-the-Man-with-the-Ball (Tag) are follow-up modes.
 */
UCLASS()
class M1UE5_API AM1DeathmatchGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AM1DeathmatchGameMode();

	/** Called from AM1Character::TakeDamage when a player dies in this mode: credits
	 *  the killer a frag (suicide/environment = -1), respawns the victim at a random
	 *  PlayerStart with full shields, and logs the running scoreboard. */
	void HandlePlayerDeath(AM1Character* Victim, AController* Killer);

	/** Spread players across the map's PlayerStarts (round-robin) instead of stacking
	 *  everyone on the first one. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Log the current scoreboard (M1Frags harness reaches it via the auth GM). */
	void LogScoreboard() const;

private:
	/** Frags per controller (kills). */
	UPROPERTY()
	TMap<TObjectPtr<AController>, int32> Frags;

	int32 NextStart = 0;  // round-robin index into the PlayerStarts
};
