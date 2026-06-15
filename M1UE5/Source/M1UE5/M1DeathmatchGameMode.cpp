#include "M1DeathmatchGameMode.h"
#include "M1Character.h"
#include "M1PlayerController.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Controller.h"

AM1DeathmatchGameMode::AM1DeathmatchGameMode()
{
	DefaultPawnClass = AM1Character::StaticClass();
	PlayerControllerClass = AM1PlayerController::StaticClass();
	// No campaign HUD bezel override needed; AM1Character's HUD is set by the
	// player controller path. Bots/teams are later; v1 is free-for-all.
}

AActor* AM1DeathmatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	if (World)
	{
		// Round-robin across all PlayerStarts so co-op/DM players don't stack.
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

void AM1DeathmatchGameMode::HandlePlayerDeath(AM1Character* Victim, AController* Killer)
{
	if (!Victim)
	{
		return;
	}
	AController* VictimCtrl = Victim->GetController();

	// Score: a kill credits the killer; a suicide / environment death costs a frag.
	if (Killer && Killer != VictimCtrl)
	{
		Frags.FindOrAdd(Killer) += 1;
	}
	else if (VictimCtrl)
	{
		int32& F = Frags.FindOrAdd(VictimCtrl);
		F = FMath::Max(0, F - 1);
	}

	// Respawn: heal to full and teleport to a fresh PlayerStart (instant respawn).
	if (AActor* Start = ChoosePlayerStart(VictimCtrl))
	{
		Victim->SetActorLocationAndRotation(Start->GetActorLocation(), Start->GetActorRotation(),
			false, nullptr, ETeleportType::TeleportPhysics);
	}
	Victim->M1Respawn();

	LogScoreboard();
}

void AM1DeathmatchGameMode::LogScoreboard() const
{
	int32 i = 0;
	for (const TPair<TObjectPtr<AController>, int32>& Pair : Frags)
	{
		const FString Who = Pair.Key ? Pair.Key->GetName() : TEXT("?");
		UE_LOG(LogTemp, Warning, TEXT("[M1Frags] #%d %s = %d"), i++, *Who, Pair.Value);
	}
	UE_LOG(LogTemp, Warning, TEXT("[M1Frags] players=%d"), Frags.Num());
}
