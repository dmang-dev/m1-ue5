#pragma once

#include "CoreMinimal.h"
#include "M1DeathmatchGameMode.h"
#include "M1TagGameMode.generated.h"

class AM1Ball;
class AM1Character;

/**
 * Kill the Man with the Ball ("Tag"): grab the ball by touching it and score while
 * you hold it. Frag the carrier and the ball DROPS where they died -- anyone can
 * scramble for it. Score = time as carrier. The ball is an authored/auto-spawned
 * AM1Ball actor.
 * Select: ...?game=/Script/M1UE5.M1TagGameMode?listen
 */
UCLASS()
class M1UE5_API AM1TagGameMode : public AM1DeathmatchGameMode
{
	GENERATED_BODY()

public:
	virtual FString GetModeName() const override { return TEXT("TAG"); }

	/** The ball calls this when a player touches it while it's neutral. */
	void OnBallTouched(AM1Character* NewCarrier);

protected:
	virtual void BeginPlay() override;
	/** Killing the carrier drops the ball where they fell (no auto-transfer). */
	virtual void AwardKill(AController* Killer, AController* Victim) override;
	virtual void ScoreTick(float DeltaSeconds) override;

private:
	void EnsureBall();
	void DropBall(const FVector& At);

	UPROPERTY()
	TObjectPtr<AM1Ball> Ball = nullptr;
	UPROPERTY()
	TObjectPtr<AController> Carrier = nullptr;
	float CarryTime = 0.0f;  // fractional seconds-as-carrier accumulator
};
