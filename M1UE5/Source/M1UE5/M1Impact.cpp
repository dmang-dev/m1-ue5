#include "M1Impact.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AM1Impact::AM1Impact()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (UStaticMesh* SphereMesh = SphereFinder.Get())
	{
		Mesh->SetStaticMesh(SphereMesh);
	}
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> MatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MatFinder.Get();

	InitialLifeSpan = Life;
}

void AM1Impact::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	// Shrink to nothing over the lifetime so the spark "pops" and fades.
	const float T = FMath::Clamp(Age / FMath::Max(0.01f, Life), 0.0f, 1.0f);
	const float S = StartScale * (1.0f - T);
	SetActorScale3D(FVector(FMath::Max(0.001f, S)));
}

void AM1Impact::Spawn(UWorld* World, const FVector& Location, float RadiusCm,
	const FLinearColor& Color)
{
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AM1Impact* Impact = World->SpawnActor<AM1Impact>(
		AM1Impact::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (!Impact)
	{
		return;
	}
	// Engine sphere is 100cm diameter at scale 1, so scale = RadiusCm*2/100.
	Impact->StartScale = FMath::Max(0.04f, (RadiusCm * 2.0f) / 100.0f);
	Impact->SetActorScale3D(FVector(Impact->StartScale));
	if (Impact->Mesh && Impact->BaseMaterial)
	{
		if (UMaterialInstanceDynamic* MID =
			UMaterialInstanceDynamic::Create(Impact->BaseMaterial, Impact))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
			Impact->Mesh->SetMaterial(0, MID);
		}
	}
}
