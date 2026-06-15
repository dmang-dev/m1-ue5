#include "M1Panel.h"
#include "M1Character.h"
#include "M1Platform.h"
#include "M1Elevator.h"
#include "M1Light.h"
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
		: Kind == 4 ? TEXT("tag-switch") : Kind == 5 ? TEXT("light-switch") : TEXT("?");
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
	// Platform switches can also target wide platforms (ride-on elevators).
	for (TActorIterator<AM1Elevator> It(W); It; ++It)
	{
		const bool bMatch = (Kind == 3)
			? ((*It)->GetPolyIndex() == Target)
			: (Target >= 1 && (*It)->GetTag() == Target);
		if (bMatch)
		{
			(*It)->ForceActivate();
			++Opened;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("M1Panel switch kind=%d target=%d -> activated %d platform(s)"),
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
	else if (Kind == 5)
	{
		// Light switch: toggle every per-room light on this LITE channel (Target =
		// LITE index). A channel is shared level-wide, so this flips all rooms on
		// that channel -- faithful to Marathon, where a light is a global channel.
		int32 Toggled = 0;
		if (UWorld* W = GetWorld())
		{
			for (TActorIterator<AM1Light> It(W); It; ++It)
			{
				if ((*It)->GetLiteIndex() == Target)
				{
					(*It)->Toggle();
					++Toggled;
				}
			}
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(20, 2.5f, FColor::Yellow,
				FString::Printf(TEXT("Light switch (%d light%s)"),
					Toggled, Toggled == 1 ? TEXT("") : TEXT("s")));
		}
	}
	else
	{
		// Pattern buffer: full save-anywhere snapshot (player pose/vitals/loadout +
		// living enemies), like Marathon's save terminals. M1SaveFull also keeps the
		// checkpoint fields valid (so menu Continue still works) and shows its own
		// "Game saved" message.
		Player->M1SaveFull();
	}

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(),
			FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
	}
	return true;
}
