#include "M1Panel.h"
#include "M1Character.h"
#include "M1Audio.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

AM1Panel::AM1Panel()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		Mesh->SetMaterial(0, MatFinder.Object);
	}
	// Decorative marker; interaction is use-activated, not collision-driven.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AM1Panel::InitFromPanel(const FM1Panel& Panel)
{
	Kind = Panel.Kind;
	Cap = Panel.Cap;
	// Small wall plaque (~48 x 14 x 56 cm). Chargers a touch taller than buffers
	// so the two read differently until real panel art lands.
	// Chargers tallest, oxygen mid, buffers shortest, so the three read apart
	// until real panel art lands.
	Mesh->SetWorldScale3D(Kind == 0 ? FVector(0.48f, 0.14f, 0.56f)
	                    : Kind == 2 ? FVector(0.48f, 0.14f, 0.48f)
	                                : FVector(0.48f, 0.14f, 0.40f));
	SetActorLocation(FVector(Panel.X, Panel.Y, Panel.Z));

	// Charger/oxygen = powerup chime; pattern buffer = item chime.
	const TCHAR* SoundPath = (Kind == 1)
		? TEXT("/Game/Audio/Monster/A_got_item.A_got_item")
		: TEXT("/Game/Audio/Monster/A_got_powerup.A_got_powerup");
	Sound = LoadObject<USoundBase>(nullptr, SoundPath);
}

bool AM1Panel::Activate(AM1Character* Player)
{
	if (!Player)
	{
		return false;
	}

	if (Kind == 0)
	{
		// Shield recharger: top up toward the panel's cap (1x/2x/3x), never down.
		Player->RefillShields(Cap);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(20, 2.5f, FColor::Cyan, TEXT("Shields recharged"));
		}
	}
	else if (Kind == 2)
	{
		// Oxygen recharger: refill the suit toward full (matters on vacuum levels).
		Player->RefillOxygen(Cap);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(20, 2.5f, FColor(0, 220, 60), TEXT("Oxygen recharged"));
		}
	}
	else
	{
		// Pattern buffer: checkpoint the current level at the player's shields.
		const int32 Idx = GetWorld()
			? AM1Character::M1LevelIndexForMap(GetWorld()->GetMapName()) : -1;
		if (Idx >= 0)
		{
			AM1Character::M1WriteCheckpoint(Idx, Player->GetHealth());
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(20, 2.5f, FColor::Green, TEXT("Game saved (pattern buffer)"));
		}
	}

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(),
			FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
	}
	return true;
}
