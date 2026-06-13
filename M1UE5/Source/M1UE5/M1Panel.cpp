#include "M1Panel.h"
#include "M1Character.h"
#include "M1Platform.h"
#include "M1Audio.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"  // TActorIterator

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
	Target = Panel.Target;
	// Wall plaque; kinds read apart by size until real panel art lands: charger
	// tallest, oxygen mid, switch a small square, buffer shortest.
	Mesh->SetWorldScale3D(Kind == 0 ? FVector(0.48f, 0.14f, 0.56f)
	                    : Kind == 2 ? FVector(0.48f, 0.14f, 0.48f)
	                    : Kind >= 3 ? FVector(0.32f, 0.14f, 0.32f)
	                                : FVector(0.48f, 0.14f, 0.40f));
	SetActorLocation(FVector(Panel.X, Panel.Y, Panel.Z));

	// Switch = click; pattern buffer = item chime; charger/oxygen = powerup chime.
	const TCHAR* SoundPath = (Kind >= 3)
		? TEXT("/Game/Audio/Misc/A_pfhor_switch_on.A_pfhor_switch_on")
		: (Kind == 1) ? TEXT("/Game/Audio/Monster/A_got_item.A_got_item")
		              : TEXT("/Game/Audio/Monster/A_got_powerup.A_got_powerup");
	Sound = LoadObject<USoundBase>(nullptr, SoundPath);
}

FString AM1Panel::M1DebugString() const
{
	const TCHAR* K = Kind == 0 ? TEXT("shield-charger") : Kind == 1 ? TEXT("pattern-buffer")
		: Kind == 2 ? TEXT("oxygen-recharger") : Kind == 3 ? TEXT("platform-switch")
		: Kind == 4 ? TEXT("tag-switch") : TEXT("?");
	const FVector L = GetActorLocation();
	return FString::Printf(TEXT("(%.0f,%.0f,%.0f) kind=%s cap=%.0f target=%d"),
		L.X, L.Y, L.Z, K, Cap, Target);
}

int32 AM1Panel::FireSwitch()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return 0;
	}
	int32 Opened = 0;
	for (TActorIterator<AM1Platform> It(W); It; ++It)
	{
		// Kind 3 = platform switch: match the door's source polygon. Kind 4 = tag
		// switch: match the door's tag (ignore tag 0 / "untagged" to avoid a
		// mistuned switch flinging every door open).
		const bool bMatch = (Kind == 3)
			? ((*It)->GetPolyIndex() == Target)
			: (Target >= 1 && (*It)->GetTag() == Target);
		if (bMatch)
		{
			(*It)->ForceOpen();
			++Opened;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("M1Panel switch kind=%d target=%d -> opened %d door(s)"),
		Kind, Target, Opened);
	return Opened;
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
	else if (Kind == 3 || Kind == 4)
	{
		// Control switch: force-open the door(s) it targets.
		const int32 Opened = FireSwitch();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(20, 2.5f, FColor::Yellow,
				FString::Printf(TEXT("Switch (opened %d door%s)"), Opened, Opened == 1 ? TEXT("") : TEXT("s")));
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
