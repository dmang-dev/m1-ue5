#include "M1Hill.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AM1Hill::AM1Hill()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;          // so late-joining clients see an auto-spawned hill

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	RootComponent = Disc;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded())
	{
		Disc->SetStaticMesh(Cyl.Object);
	}
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCastShadow(false);
}

void AM1Hill::BeginPlay()
{
	Super::BeginPlay();

	// The BasicShapes cylinder is 100cm tall / 100cm diameter (radius 50). Scale XY so
	// the disc diameter matches 2*Radius, and flatten it to a low pad on the floor.
	const float XY = (Radius * 2.0f) / 100.0f;
	Disc->SetWorldScale3D(FVector(XY, XY, 0.08f));

	// Gold, glowy hill pad (BasicShapeMaterial exposes a "Color" param).
	Disc->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(0.95f, 0.78f, 0.10f));
}
