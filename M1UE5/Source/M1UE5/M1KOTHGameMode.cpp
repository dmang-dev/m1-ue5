#include "M1KOTHGameMode.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

void AM1KOTHGameMode::ScoreTick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!bHillSet)
	{
		// The hill = centroid of all PlayerStarts (no authored hill volume yet).
		FVector Sum = FVector::ZeroVector;
		int32 N = 0;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Sum += It->GetActorLocation();
			++N;
		}
		HillCenter = (N > 0) ? (Sum / N) : FVector::ZeroVector;
		bHillSet = true;
		UE_LOG(LogTemp, Warning, TEXT("[M1Net] KOTH hill at (%.0f,%.0f,%.0f) r=%.0f"),
			HillCenter.X, HillCenter.Y, HillCenter.Z, HillRadius);
	}
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
