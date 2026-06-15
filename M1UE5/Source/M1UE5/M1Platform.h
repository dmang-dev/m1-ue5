#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M1PlatformTable.h"
#include "M1Platform.generated.h"

class UStaticMeshComponent;
class USoundBase;

/**
 * A Marathon door: a movable box panel that fills the doorway when closed and
 * sinks below the floor when open. Auto-opens when the player is near, holds for
 * a dwell, then closes (re-opening if the player returns mid-close). Spawned at
 * level load by AM1GameMode from the generated M1PlatformTable.h.
 */
UCLASS()
class M1UE5_API AM1Platform : public AActor
{
	GENERATED_BODY()

public:
	AM1Platform();

	/** Configure geometry + motion from a generated door record. */
	void InitFromDoor(const FM1Door& Door);

	virtual void Tick(float DeltaSeconds) override;

	/** Debug line for the test harness: position + open/closed state. */
	FString M1DebugString() const;

	/** Open now regardless of proximity (a control switch fired). Auto-closes on
	 *  the usual dwell afterward. No-op if already open/opening. */
	void ForceOpen();

	int32 GetPolyIndex() const { return PolyIndex; }
	int32 GetTag() const { return Tag; }

private:
	UPROPERTY()
	UStaticMeshComponent* Panel = nullptr;

	USoundBase* OpenSound = nullptr;
	USoundBase* CloseSound = nullptr;

	enum class EDoorState : uint8 { Closed, Opening, Open, Closing };
	EDoorState State = EDoorState::Closed;

	float ClosedZ = 0.0f;        // panel centre when sealed
	float OpenZ = 0.0f;          // panel centre when fully retracted
	float Speed = 100.0f;        // cm/s
	float Dwell = 3.0f;          // seconds held open
	float OpenTimer = 0.0f;
	float ActivateRadius = 170.0f;
	float VerticalReach = 300.0f;
	int32 PolyIndex = -1;        // source polygon (platform-switch target)
	int32 Tag = -1;              // platform tag (tag-switch target)

	bool PlayerNear() const;
	/** Play the open/close one-shot on ALL machines so co-op clients hear the door.
	 *  NetMulticast -> must be invoked from the server (authority). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDoorSound(USoundBase* Sound);
};
