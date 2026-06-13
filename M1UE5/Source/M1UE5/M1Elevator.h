#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M1PlatformTable.h"
#include "M1Elevator.generated.h"

class UStaticMeshComponent;
class USoundBase;

/**
 * A Marathon ride-on elevator (wide platform): a movable floor slab whose top
 * rises from LowZ to HighZ, carrying the player (UE based-movement). Rises when
 * the player stands on it (or a switch fires it), holds, then returns. Engine
 * BasicShapes box -- no new cooked asset, no geometry rebake. v1: rises to the
 * highest neighbour floor (clamped under the ceiling); a fully faithful version
 * would author the floor as a movable mesh in the converter.
 */
UCLASS()
class M1UE5_API AM1Elevator : public AActor
{
	GENERATED_BODY()

public:
	AM1Elevator();

	void InitFromElevator(const FM1Elevator& E);
	virtual void Tick(float DeltaSeconds) override;

	/** Switch-triggered: send the lift up (same as a rider stepping on). */
	void ForceActivate();

	FString M1DebugString() const;
	int32 GetPolyIndex() const { return PolyIndex; }
	int32 GetTag() const { return Tag; }

private:
	UPROPERTY()
	UStaticMeshComponent* Slab = nullptr;

	USoundBase* MoveSound = nullptr;

	enum class EState : uint8 { Rest, Rising, Up, Lowering };
	EState State = EState::Rest;

	float LowTop = 0.0f;    // slab-top Z at rest
	float HighTop = 0.0f;   // slab-top Z fully raised
	float Thick = 16.0f;    // slab thickness (cm)
	float Speed = 100.0f;   // cm/s
	float Dwell = 2.5f;     // seconds held up before returning
	float Timer = 0.0f;
	float HalfX = 100.0f;   // footprint half-extents (cm)
	float HalfY = 100.0f;
	int32 PolyIndex = -1;
	int32 Tag = -1;

	float SlabTop() const;          // current slab-top Z
	void SetSlabTop(float TopZ);    // place the slab so its top is at TopZ
	bool PlayerOn() const;          // player within the footprint, near the top
};
