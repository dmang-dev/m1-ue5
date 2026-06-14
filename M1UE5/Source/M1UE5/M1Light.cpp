#include "M1Light.h"
#include "Components/PointLightComponent.h"

AM1Light::AM1Light()
{
	PrimaryActorTick.bCanEverTick = false;

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	RootComponent = Light;
	// Runtime-spawned -> must be Movable (no baked lightmaps in the headless cook).
	Light->SetMobility(EComponentMobility::Movable);
	// Unshadowed: M1 levels carry up to ~150 of these; shadows would be the only
	// real cost, and Marathon had none. Inverse-squared falloff off for a softer,
	// more even room fill that better mimics per-surface light levels.
	Light->SetCastShadows(false);
	Light->bUseInverseSquaredFalloff = false;
	Light->LightFalloffExponent = 4.0f;
}

void AM1Light::InitFromLight(const FM1Light& L)
{
	LiteIndex = L.LiteIndex;
	OnIntensity = FMath::Max(0.0f, L.Intensity) * kIntensityScale;
	bOn = (L.Mode != 3);  // mode 3 = becoming-inactive/off; everything else on
	SetActorLocation(FVector(L.X, L.Y, L.Z));
	Light->SetAttenuationRadius(FMath::Max(50.0f, L.Radius));
	Apply();
}

void AM1Light::Apply()
{
	if (Light)
	{
		Light->SetIntensity(bOn ? OnIntensity : 0.0f);
		Light->SetVisibility(bOn);
	}
}

bool AM1Light::Toggle()
{
	bOn = !bOn;
	Apply();
	return bOn;
}

void AM1Light::SetOn(bool bNewOn)
{
	bOn = bNewOn;
	Apply();
}

FString AM1Light::M1DebugString() const
{
	const FVector P = GetActorLocation();
	return FString::Printf(TEXT("(%.0f,%.0f,%.0f) lite=%d intensity=%.0f on=%d"),
		P.X, P.Y, P.Z, LiteIndex, OnIntensity, bOn ? 1 : 0);
}
