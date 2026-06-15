#include "M1DeathmatchGameMode.h"
#include "M1Character.h"
#include "M1PlayerController.h"
#include "M1PlayerState.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

AM1DeathmatchGameMode::AM1DeathmatchGameMode()
{
	DefaultPawnClass = AM1Character::StaticClass();
	PlayerControllerClass = AM1PlayerController::StaticClass();
	PlayerStateClass = AM1PlayerState::StaticClass();   // replicated TeamNum + color
	PrimaryActorTick.bCanEverTick = true;       // time-based modes (KOTH/Tag) score on tick
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AM1DeathmatchGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);
	if (NumTeams >= 2 && NewPlayer)
	{
		if (AM1PlayerState* PS = NewPlayer->GetPlayerState<AM1PlayerState>())
		{
			// Round-robin onto the smallest team (simple balance via login order).
			PS->TeamNum = NextStart % NumTeams;
		}
	}
}

AActor* AM1DeathmatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	if (World)
	{
		TArray<APlayerStart*> Starts;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Starts.Add(*It);
		}
		if (Starts.Num() > 0)
		{
			APlayerStart* Pick = Starts[NextStart % Starts.Num()];
			++NextStart;
			return Pick;
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AM1DeathmatchGameMode::AddScore(AController* C, int32 Delta)
{
	if (!C || !C->PlayerState)
	{
		return;
	}
	const int32 NewScore = FMath::Max(0, FMath::RoundToInt(C->PlayerState->GetScore()) + Delta);
	C->PlayerState->SetScore(static_cast<float>(NewScore));
	if (!bRoundOver && NewScore >= ScoreLimit)
	{
		EndRound(C);
	}
}

void AM1DeathmatchGameMode::AwardKill(AController* Killer, AController* Victim)
{
	if (Killer && Killer != Victim)
	{
		AddScore(Killer, +1);          // a frag
	}
	else if (Victim)
	{
		AddScore(Victim, -1);          // suicide / environment death
	}
}

void AM1DeathmatchGameMode::HandlePlayerDeath(AM1Character* Victim, AController* Killer)
{
	if (!Victim || bRoundOver)
	{
		return;
	}
	AController* VictimCtrl = Victim->GetController();
	AwardKill(Killer, VictimCtrl);

	// Instant respawn: teleport to a fresh start + heal.
	if (AActor* Start = ChoosePlayerStart(VictimCtrl))
	{
		Victim->SetActorLocationAndRotation(Start->GetActorLocation(), Start->GetActorRotation(),
			false, nullptr, ETeleportType::TeleportPhysics);
	}
	Victim->M1Respawn();
}

void AM1DeathmatchGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bRoundOver)
	{
		ScoreTick(DeltaSeconds);
	}
}

void AM1DeathmatchGameMode::EndRound(AController* Winner)
{
	bRoundOver = true;
	const FString Name = (Winner && Winner->PlayerState)
		? Winner->PlayerState->GetPlayerName() : TEXT("?");
	UE_LOG(LogTemp, Warning, TEXT("[M1Net] %s ROUND OVER -- winner: %s (%d)"),
		*GetModeName(), *Name, Winner && Winner->PlayerState
			? FMath::RoundToInt(Winner->PlayerState->GetScore()) : 0);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(30, 8.0f, FColor::Yellow,
			FString::Printf(TEXT("%s WINNER: %s"), *GetModeName(), *Name));
	}
	// Restart the same map after a short delay (PlayerStates -> scores reset on travel).
	if (UWorld* World = GetWorld())
	{
		FString Map = World->GetMapName();
		Map.RemoveFromStart(World->StreamingLevelsPrefix);
		FTimerHandle Th;
		World->GetTimerManager().SetTimer(Th, FTimerDelegate::CreateLambda([World, Map]()
		{
			World->ServerTravel(Map + TEXT("?listen"), false);
		}), 6.0f, false);
	}
}
