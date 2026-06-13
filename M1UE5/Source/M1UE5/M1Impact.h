#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M1Impact.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 * A brief emissive impact spark: a small unlit-emissive sphere that pops at a
 * hit point and shrinks away over a fraction of a second, then destroys itself.
 * Spawned by weapon hitscan / projectile impacts as lightweight v1 hit feedback
 * (the prior debug spheres are now editor-only). Engine BasicShapes only -- no
 * new cooked asset. Persistent scorch DECALS are a later refinement.
 */
UCLASS()
class M1UE5_API AM1Impact : public AActor
{
	GENERATED_BODY()

public:
	AM1Impact();

	virtual void Tick(float DeltaSeconds) override;

	/** Spawn a spark at a world location. Radius in cm; Color is the flash tint. */
	static void Spawn(UWorld* World, const FVector& Location, float RadiusCm,
		const FLinearColor& Color);

private:
	UPROPERTY()
	UStaticMeshComponent* Mesh = nullptr;

	UPROPERTY()
	UMaterialInterface* BaseMaterial = nullptr;

	float StartScale = 0.2f;
	float Life = 0.18f;   // seconds; matches InitialLifeSpan
	float Age = 0.0f;
};
