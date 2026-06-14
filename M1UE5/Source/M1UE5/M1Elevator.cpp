#include "M1Elevator.h"
#include "M1Audio.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

AM1Elevator::AM1Elevator()
{
	PrimaryActorTick.bCanEverTick = true;

	Slab = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Slab"));
	RootComponent = Slab;
	Slab->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Slab->SetStaticMesh(CubeFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		Slab->SetMaterial(0, MatFinder.Object);
	}
	// Solid floor: block everything so the player rides it and shots don't pass.
	Slab->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Slab->SetCollisionObjectType(ECC_WorldStatic);
	Slab->SetCollisionResponseToAllChannels(ECR_Block);
	Slab->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<USoundBase> MoveS(
		TEXT("/Game/Audio/Misc/A_pfhor_platform_starting.A_pfhor_platform_starting"));
	if (MoveS.Succeeded())
	{
		MoveSound = MoveS.Object;
	}
}

void AM1Elevator::InitFromElevator(const FM1Elevator& E)
{
	HalfX = FMath::Max(E.SizeX, 20.0f) * 0.5f;
	HalfY = FMath::Max(E.SizeY, 20.0f) * 0.5f;
	LowTop = E.LowZ;
	HighTop = E.HighZ;
	Speed = FMath::Max(E.Speed, 20.0f);
	PolyIndex = E.PolyIndex;
	Tag = E.Tag;

	// Engine cube is 100 cm; scale to footprint x thickness.
	Slab->SetWorldScale3D(FVector(HalfX * 2.0f / 100.0f, HalfY * 2.0f / 100.0f, Thick / 100.0f));

	// Skin the lift with its Marathon floor texture; prefer the lit/hybrid library
	// so it matches lit sector floors, falling back to emissive on flat builds.
	if (E.TexColl >= 0 && E.TexShape >= 0)
	{
		const FString Hybrid = FString::Printf(
			TEXT("/Game/Materials_hybrid/tex_%d_%d.tex_%d_%d"),
			E.TexColl, E.TexShape, E.TexColl, E.TexShape);
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *Hybrid);
		if (!M)
		{
			const FString Emissive = FString::Printf(
				TEXT("/Game/Materials/tex_%d_%d.tex_%d_%d"),
				E.TexColl, E.TexShape, E.TexColl, E.TexShape);
			M = LoadObject<UMaterialInterface>(nullptr, *Emissive);
		}
		if (M)
		{
			Slab->SetMaterial(0, M);
		}
	}

	SetActorLocation(FVector(E.X, E.Y, 0.0f));
	SetSlabTop(LowTop);
	State = EState::Rest;
}

float AM1Elevator::SlabTop() const
{
	return GetActorLocation().Z + Thick * 0.5f;
}

void AM1Elevator::SetSlabTop(float TopZ)
{
	// No sweep: a swept move collides with the rider standing on the slab and gets
	// blocked. Moving freely lets UE based-movement carry the player with the slab.
	const FVector L = GetActorLocation();
	SetActorLocation(FVector(L.X, L.Y, TopZ - Thick * 0.5f), /*bSweep*/ false);
}

bool AM1Elevator::PlayerOn() const
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return false;
	}
	const FVector P = Player->GetActorLocation();
	const FVector C = GetActorLocation();
	// Within the footprint (small margin) and standing on/just above the slab top.
	if (FMath::Abs(P.X - C.X) > HalfX + 25.0f || FMath::Abs(P.Y - C.Y) > HalfY + 25.0f)
	{
		return false;
	}
	const float dz = P.Z - SlabTop();
	return dz > -20.0f && dz < 160.0f;
}

void AM1Elevator::ForceActivate()
{
	if (State == EState::Rest || State == EState::Lowering)
	{
		State = EState::Rising;
		if (MoveSound) { UGameplayStatics::PlaySoundAtLocation(this, MoveSound, GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation()); }
	}
	else if (State == EState::Up)
	{
		Timer = 0.0f;  // refresh the hold
	}
}

void AM1Elevator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Step = Speed * DeltaSeconds;
	const float Top = SlabTop();

	switch (State)
	{
	case EState::Rest:
		if (PlayerOn())
		{
			State = EState::Rising;
			if (MoveSound) { UGameplayStatics::PlaySoundAtLocation(this, MoveSound, GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation()); }
		}
		break;

	case EState::Rising:
	{
		const float NewTop = FMath::Min(HighTop, Top + Step);
		SetSlabTop(NewTop);
		if (NewTop >= HighTop - 0.5f)
		{
			State = EState::Up;
			Timer = 0.0f;
		}
		break;
	}

	case EState::Up:
		Timer += DeltaSeconds;
		if (!PlayerOn() && Timer >= Dwell)
		{
			State = EState::Lowering;
		}
		break;

	case EState::Lowering:
	{
		const float NewTop = FMath::Max(LowTop, Top - Step);
		SetSlabTop(NewTop);
		if (NewTop <= LowTop + 0.5f)
		{
			State = EState::Rest;
		}
		break;
	}
	}
}

FString AM1Elevator::M1DebugString() const
{
	const TCHAR* S =
		State == EState::Rest ? TEXT("REST") :
		State == EState::Rising ? TEXT("RISING") :
		State == EState::Up ? TEXT("UP") : TEXT("LOWERING");
	const FVector L = GetActorLocation();
	return FString::Printf(TEXT("(%.0f,%.0f) top=%.0f low=%.0f high=%.0f poly=%d tag=%d [%s]"),
		L.X, L.Y, SlabTop(), LowTop, HighTop, PolyIndex, Tag, S);
}
