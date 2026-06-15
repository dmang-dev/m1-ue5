#include "M1Ball.h"
#include "M1TagGameMode.h"
#include "M1Character.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "UObject/ConstructorHelpers.h"

AM1Ball::AM1Ball()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);  // clients see it drop/follow the carrier

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->InitSphereRadius(60.0f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.45f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void AM1Ball::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(0.85f, 0.05f, 0.85f));
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AM1Ball::OnOverlap);
	}
}

void AM1Ball::OnOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
{
	if (!HasAuthority() || bHeld)
	{
		return;  // already carried; ignore until dropped
	}
	if (AM1Character* Char = Cast<AM1Character>(Other))
	{
		if (TagMode.IsValid())
		{
			TagMode->OnBallTouched(Char);  // mode sets carrier + calls AttachToCarrier
		}
	}
}

void AM1Ball::AttachToCarrier(AM1Character* Carrier)
{
	if (!Carrier)
	{
		return;
	}
	bHeld = true;
	AttachToComponent(Carrier->GetCapsuleComponent(),
		FAttachmentTransformRules::SnapToTargetIncludingScale);
	SetActorRelativeLocation(FVector(0.0f, 0.0f, 110.0f));  // bob above the head
}

void AM1Ball::DropAt(const FVector& Location)
{
	bHeld = false;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocation(Location + FVector(0.0f, 0.0f, 40.0f));
}
