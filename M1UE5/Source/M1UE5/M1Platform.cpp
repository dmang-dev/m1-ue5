#include "M1Platform.h"
#include "M1Audio.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

AM1Platform::AM1Platform()
{
	PrimaryActorTick.bCanEverTick = true;

	Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	RootComponent = Panel;
	Panel->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Panel->SetStaticMesh(CubeFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		Panel->SetMaterial(0, MatFinder.Object);
	}

	// Block the player, enemies, and shots while closed; moves with the panel.
	Panel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Panel->SetCollisionObjectType(ECC_WorldStatic);
	Panel->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<USoundBase> OpenS(
		TEXT("/Game/Audio/Monster/A_spht_door_opening.A_spht_door_opening"));
	if (OpenS.Succeeded())
	{
		OpenSound = OpenS.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundBase> CloseS(
		TEXT("/Game/Audio/Monster/A_spht_door_closing.A_spht_door_closing"));
	if (CloseS.Succeeded())
	{
		CloseSound = CloseS.Object;
	}
}

void AM1Platform::InitFromDoor(const FM1Door& Door)
{
	// The engine cube is 100 cm; scale to the door's box dimensions.
	Panel->SetWorldScale3D(FVector(
		FMath::Max(Door.SizeX, 1.0f) / 100.0f,
		FMath::Max(Door.SizeY, 1.0f) / 100.0f,
		FMath::Max(Door.SizeZ, 1.0f) / 100.0f));

	ClosedZ = Door.Z;
	OpenZ = Door.Z - Door.SizeZ;   // sink a full height -> top flush with the floor
	Speed = FMath::Max(Door.Speed, 20.0f);
	Dwell = Door.Dwell;
	// Trigger a little beyond the panel's larger footprint so it opens on walk-up.
	ActivateRadius = FMath::Max(Door.SizeX, Door.SizeY) * 0.5f + 170.0f;
	VerticalReach = 300.0f + Door.SizeZ;

	PolyIndex = Door.PolyIndex;
	Tag = Door.Tag;

	SetActorLocation(FVector(Door.X, Door.Y, ClosedZ));
	State = EDoorState::Closed;
}

void AM1Platform::ForceOpen()
{
	if (State == EDoorState::Open || State == EDoorState::Opening)
	{
		OpenTimer = 0.0f;  // already open(ing) -> just refresh the hold
		return;
	}
	State = EDoorState::Opening;
	PlayDoorSound(OpenSound);
}

bool AM1Platform::PlayerNear() const
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return false;
	}
	const FVector D = Player->GetActorLocation() - GetActorLocation();
	const float HorizSq = D.X * D.X + D.Y * D.Y;
	return HorizSq <= ActivateRadius * ActivateRadius && FMath::Abs(D.Z) < VerticalReach;
}

void AM1Platform::PlayDoorSound(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(),
			FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
	}
}

FString AM1Platform::M1DebugString() const
{
	const TCHAR* S =
		State == EDoorState::Closed ? TEXT("CLOSED") :
		State == EDoorState::Opening ? TEXT("OPENING") :
		State == EDoorState::Open ? TEXT("OPEN") : TEXT("CLOSING");
	const FVector L = GetActorLocation();
	return FString::Printf(TEXT("(%.0f,%.0f,%.0f) Z=%.0f closedZ=%.0f openZ=%.0f poly=%d tag=%d [%s]"),
		L.X, L.Y, L.Z, L.Z, ClosedZ, OpenZ, PolyIndex, Tag, S);
}

void AM1Platform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector Loc = GetActorLocation();
	const float Step = Speed * DeltaSeconds;

	switch (State)
	{
	case EDoorState::Closed:
		if (PlayerNear())
		{
			State = EDoorState::Opening;
			PlayDoorSound(OpenSound);
		}
		break;

	case EDoorState::Opening:
	{
		const float NewZ = FMath::Max(OpenZ, Loc.Z - Step);
		SetActorLocation(FVector(Loc.X, Loc.Y, NewZ));
		if (NewZ <= OpenZ + 0.5f)
		{
			State = EDoorState::Open;
			OpenTimer = 0.0f;
		}
		break;
	}

	case EDoorState::Open:
		OpenTimer += DeltaSeconds;
		if (!PlayerNear() && OpenTimer >= Dwell)
		{
			State = EDoorState::Closing;
			PlayDoorSound(CloseSound);
		}
		break;

	case EDoorState::Closing:
		if (PlayerNear())   // don't crush -- re-open if the player steps back in
		{
			State = EDoorState::Opening;
			PlayDoorSound(OpenSound);
			break;
		}
		{
			const float NewZ = FMath::Min(ClosedZ, Loc.Z + Step);
			SetActorLocation(FVector(Loc.X, Loc.Y, NewZ));
			if (NewZ >= ClosedZ - 0.5f)
			{
				State = EDoorState::Closed;
			}
		}
		break;
	}
}
