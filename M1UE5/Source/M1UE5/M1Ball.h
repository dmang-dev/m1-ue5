#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M1Ball.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AM1TagGameMode;
class AM1Character;

/**
 * "Kill the Man with the Ball" ball. A neutral floating skull players grab by
 * touching it; the carrier scores while holding it and drops it where they die,
 * leaving it for anyone to pick up. Server-authoritative; attaches to the carrier
 * pawn (attachment replicates to clients). The Tag game mode owns carrier/scoring
 * state and is notified on pickup via OnBallTouched.
 */
UCLASS()
class M1UE5_API AM1Ball : public AActor
{
	GENERATED_BODY()

public:
	AM1Ball();

	/** Set by the Tag game mode when it spawns the ball. */
	void SetTagMode(AM1TagGameMode* InMode) { TagMode = InMode; }

	/** Attach above a carrier pawn (server). */
	void AttachToCarrier(AM1Character* Carrier);

	/** Detach and rest on the floor at a drop location, neutral again (server). */
	void DropAt(const FVector& Location);

	bool IsHeld() const { return bHeld; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep,
		const FHitResult& Sweep);

	UPROPERTY(VisibleAnywhere)
	USphereComponent* Trigger = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AM1TagGameMode> TagMode;

	bool bHeld = false;
};
