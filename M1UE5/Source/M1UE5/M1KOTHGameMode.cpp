#include "M1KOTHGameMode.h"
#include "M1Hill.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

void AM1KOTHGameMode::EnsureHill()
{
	if (bHillResolved)
	{
		return;
	}
	bHillResolved = true;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Prefer an authored hill placed in the level.
	for (TActorIterator<AM1Hill> It(World); It; ++It)
	{
		Hill = *It;
		break;
	}
	if (!Hill)
	{
		// None authored: spawn a visible one at the PlayerStarts centroid.
		FVector Sum = FVector::ZeroVector;
		int32 N = 0;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Sum += It->GetActorLocation();
			++N;
		}
		const FVector Center = (N > 0) ? (Sum / N) : FVector::ZeroVector;
		Hill = World->SpawnActor<AM1Hill>(AM1Hill::StaticClass(), Center, FRotator::ZeroRotator);
	}
	if (Hill)
	{
		const FVector L = Hill->GetActorLocation();
		UE_LOG(LogTemp, Warning, TEXT("[M1Net] KOTH hill at (%.0f,%.0f,%.0f) r=%.0f"),
			L.X, L.Y, L.Z, Hill->Radius);
	}
}

void AM1KOTHGameMode::ScoreTick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	EnsureHill();
	if (!Hill)
	{
		return;
	}
	const FVector HillCenter = Hill->GetActorLocation();
	const float HillRadius = Hill->Radius;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AController* C = It->Get();
		const APawn* P = C ? C->GetPawn() : nullptr;
		if (!P)
		{
			continue;
		}
		FVector D = P->GetActorLocation() - HillCenter;
		D.Z = 0.0f;
		if (D.SizeSquared() <= HillRadius * HillRadius)
		{
			float& T = HillTime.FindOrAdd(C);
			T += DeltaSeconds;
			if (T >= 1.0f)
			{
				const int32 Pts = FMath::FloorToInt(T);
				AddScore(C, Pts);   // +1 point per second on the hill
				T -= Pts;
			}
		}
	}
}
