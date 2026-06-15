#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "M1DeathmatchGameMode.generated.h"

class AM1Character;

/**
 * Base Marathon net mode -- "Carnage" / Every Man for Himself: free-for-all frag
 * scoring with instant respawn, to a frag limit. Score lives on the built-in
 * (replicated) APlayerState::Score, so client HUDs / M1Frags see it. KOTH and Tag
 * subclass this and override the scoring hooks. Select per-map via the URL:
 *   M1UE5.exe /Game/Levels/M1/L_30_Arena?game=/Script/M1UE5.M1DeathmatchGameMode?listen
 * Players are server-authoritative (AM1Character) and shootable (capsule blocks the
 * weapon Visibility trace), so PvP damage flows through the normal damage path.
 */
UCLASS()
class M1UE5_API AM1DeathmatchGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AM1DeathmatchGameMode();

	virtual void Tick(float DeltaSeconds) override;

	/** Called from AM1Character::TakeDamage when a player dies: scores the kill
	 *  (AwardKill), respawns the victim at a fresh PlayerStart, and checks the win. */
	void HandlePlayerDeath(AM1Character* Victim, AController* Killer);

	/** Round-robin the map's PlayerStarts instead of stacking everyone on one. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Assign a team (round-robin) when teams are enabled. */
	virtual void OnPostLogin(AController* NewPlayer) override;

	/** Short mode label for the HUD/log ("CARNAGE", "KING OF THE HILL", "TAG"). */
	virtual FString GetModeName() const { return TEXT("CARNAGE"); }

	/** 0 = free-for-all; >=2 enables teams (players assigned round-robin, friendly
	 *  fire off, bodies colored by team). Team subclasses set this in their ctor. */
	UPROPERTY(EditDefaultsOnly, Category = "M1Net")
	int32 NumTeams = 0;

protected:
	/** Score a kill. Base: +1 to the killer (suicide/environment: -1 to the victim).
	 *  KOTH overrides to no-op (only hill time scores); Tag transfers the "ball". */
	virtual void AwardKill(AController* Killer, AController* Victim);

	/** Per-tick scoring for time-based modes (KOTH hill, Tag ball). Base: nothing. */
	virtual void ScoreTick(float DeltaSeconds) {}

	/** Add to a controller's replicated PlayerState score; then check the limit. */
	void AddScore(AController* C, int32 Delta);

	/** Frags/points to win; reaching it ends the round (announce + restart). */
	UPROPERTY(EditDefaultsOnly, Category = "M1Net")
	int32 ScoreLimit = 10;

	bool bRoundOver = false;

private:
	void EndRound(AController* Winner);

	int32 NextStart = 0;  // round-robin index into the PlayerStarts
};
