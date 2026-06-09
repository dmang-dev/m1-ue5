#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "M1PlayerController.generated.h"

/**
 * Drives the menus: a main menu on the L_MainMenu level, and an Esc pause menu
 * during gameplay (so the player can quit). Built in Slate (no UMG assets).
 */
UCLASS()
class M1UE5_API AM1PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void ShowMainMenu();
	void TogglePauseMenu();
	void CloseMenu();

	void OnNewGame();
	void OnQuitGame();
	void OnReturnToMenu();

	TSharedRef<class SWidget> BuildMenu(const FText& Title, bool bPauseMenu);
	void PresentMenu(TSharedRef<class SWidget> Menu, bool bPause);

	TSharedPtr<class SWidget> MenuWidget;
	bool bMenuOpen = false;
	bool bOnMenuLevel = false;
};
