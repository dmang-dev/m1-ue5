#include "M1GameMode.h"
#include "M1Character.h"
#include "M1PlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"

AM1GameMode::AM1GameMode()
{
	DefaultPawnClass = AM1Character::StaticClass();
	PlayerControllerClass = AM1PlayerController::StaticClass();
}

AActor* AM1GameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Marathon start rooms are tight, so AGameModeBase's capsule-encroachment
	// check rejects our PlayerStart and the engine spawns at world origin (the
	// player drops into the void and falls forever). Just use the first
	// PlayerStart actor placed in the level.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			return *It;
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}
