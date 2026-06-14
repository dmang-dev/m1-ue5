#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M1LightTable.h"
#include "M1Light.generated.h"

class UPointLightComponent;

/**
 * A Marathon LITE light: one movable point light standing in for a connected
 * region of polygons that share a floor_light channel (see gen_light_assets.py).
 * Spawned at runtime by AM1GameMode (so it must be Movable -- the headless cook
 * builds no lightmaps), unshadowed for cost, and toggleable by a light switch
 * (panel_type 4 -> Kind 5) that matches LiteIndex. Visible only under lit/hybrid
 * materials; under the default unlit/emissive build it does nothing.
 */
UCLASS()
class M1UE5_API AM1Light : public AActor
{
	GENERATED_BODY()

public:
	AM1Light();

	void InitFromLight(const FM1Light& L);

	/** Flip the light on<->off (light switch). Returns the new on-state. */
	bool Toggle();
	void SetOn(bool bNewOn);
	bool IsOn() const { return bOn; }

	int32 GetLiteIndex() const { return LiteIndex; }
	FString M1DebugString() const;

	/** Candela per unit of LITE intensity (0..1). Tuned once for the look. Kept
	 *  LOW because dense levels stack 100+ overlapping fill lights -- too high
	 *  overexposes lit surfaces to a washed-out white instead of showing texture. */
	static constexpr float kIntensityScale = 420.0f;

private:
	UPROPERTY()
	UPointLightComponent* Light = nullptr;

	int32 LiteIndex = -1;
	float OnIntensity = 0.0f;  // candela when on
	bool bOn = true;

	void Apply();
};
