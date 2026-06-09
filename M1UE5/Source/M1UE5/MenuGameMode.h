#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MenuGameMode.generated.h"

/** GameMode for the L_MainMenu level: a floating spectator pawn (no falling) and
 *  the M1 player controller, which shows the main menu. */
UCLASS()
class M1UE5_API AMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMenuGameMode();
};
