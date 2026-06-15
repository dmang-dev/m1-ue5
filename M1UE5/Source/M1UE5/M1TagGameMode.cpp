#include "M1TagGameMode.h"
#include "M1Ball.h"
#include "M1Character.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

void AM1TagGameMode::BeginPlay()
{
	Super::BeginPlay();
	EnsureBall();
}

void AM1TagGameMode::EnsureBall()
{
	if (Ball)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Prefer an authored ball; else spawn one at the PlayerStarts centroid.
	for (TActorIterator<AM1Ball> It(World); It; ++It)
	{
		Ball = *It;
		break;
	}
	if (!Ball)
	{
		FVector Sum = FVector::ZeroVector;
		int32 N = 0;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Sum += It->GetActorLocation();
			++N;
		}
		const FVector Center = (N > 0) ? (Sum / N) : FVector::ZeroVector;
		Ball = World->SpawnActor<AM1Ball>(AM1Ball::StaticClass(),
			Center + FVector(0.0f, 0.0f, 60.0f), FRotator::ZeroRotator);
	}
	if (Ball)
	{
		Ball->SetTagMode(this);
		const FVector L = Ball->GetActorLocation();
		UE_LOG(LogTemp, Warning, TEXT("[M1Net] TAG ball at (%.0f,%.0f,%.0f)"), L.X, L.Y, L.Z);
	}
}

void AM1TagGameMode::OnBallTouched(AM1Character* NewCarrier)
{
	if (!NewCarrier || !Ball || Ball->IsHeld())
	{
		return;
	}
	Carrier = NewCarrier->GetController();
	CarryTime = 0.0f;
	Ball->AttachToCarrier(NewCarrier);
	if (Carrier && Carrier->PlayerState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[M1Net] TAG ball grabbed by %s"),
			*Carrier->PlayerState->GetPlayerName());
	}
}

void AM1TagGameMode::DropBall(const FVector& At)
{
	if (Ball)
	{
		Ball->DropAt(At);
	}
	Carrier = nullptr;
	CarryTime = 0.0f;
	UE_LOG(LogTemp, Warning, TEXT("[M1Net] TAG ball dropped at (%.0f,%.0f,%.0f)"),
		At.X, At.Y, At.Z);
}

void AM1TagGameMode::AwardKill(AController* Killer, AController* Victim)
{
	// Only the carrier dying matters: the ball drops where they fell, neutral again.
	if (Victim && Victim == Carrier)
	{
		const APawn* P = Victim->GetPawn();
		DropBall(P ? P->GetActorLocation() : (Ball ? Ball->GetActorLocation() : FVector::ZeroVector));
	}
}

void AM1TagGameMode::ScoreTick(float DeltaSeconds)
{
	AController* C = Carrier.Get();
	if (!C || !C->PlayerState || !C->GetPawn())
	{
		return;  // no carrier, or carrier's pawn is gone
	}
	CarryTime += DeltaSeconds;
	if (CarryTime >= 1.0f)
	{
		const int32 Pts = FMath::FloorToInt(CarryTime);
		AddScore(C, Pts);   // +1 point per second carrying the ball
		CarryTime -= Pts;
	}
}
