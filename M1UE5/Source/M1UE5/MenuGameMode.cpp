#include "MenuGameMode.h"
#include "M1PlayerController.h"
#include "GameFramework/SpectatorPawn.h"

AMenuGameMode::AMenuGameMode()
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	PlayerControllerClass = AM1PlayerController::StaticClass();
}
