#include "M1TagGameMode.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

void AM1TagGameMode::AwardKill(AController* Killer, AController* Victim)
{
	// Frag to grab the ball: the killer becomes the carrier (suicide keeps it).
	if (Killer && Killer != Victim)
	{
		Carrier = Killer;
		CarryTime = 0.0f;
		if (Killer->PlayerState)
		{
			UE_LOG(LogTemp, Warning, TEXT("[M1Net] TAG ball -> %s"),
				*Killer->PlayerState->GetPlayerName());
		}
	}
}

void AM1TagGameMode::ScoreTick(float DeltaSeconds)
{
	AController* C = Carrier.Get();
	if (!C || !C->PlayerState)
	{
		return;  // no carrier yet (first frag sets it) or carrier left
	}
	CarryTime += DeltaSeconds;
	if (CarryTime >= 1.0f)
	{
		const int32 Pts = FMath::FloorToInt(CarryTime);
		AddScore(C, Pts);   // +1 point per second carrying the ball
		CarryTime -= Pts;
	}
}
