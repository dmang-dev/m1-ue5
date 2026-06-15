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

	/** Campaign-complete screen: a green-on-black victory message, shown when the
	 *  final level's exit terminal fires (AM1Character::AdvanceLevel). Click to
	 *  return to the main menu. */
	void ShowEpilogue();

private:
	void ShowMainMenu();
	void TogglePauseMenu();
	void CloseMenu();

	// Marathon shows a full-screen chapter splash (level name baked in) when the
	// player enters a story-beat level. Shown on top, game paused, until clicked.
	void MaybeShowChapterScreen();
	void DismissChapterScreen();

	void OnNewGame();
	void OnContinue();
	void OnQuitGame();
	void OnReturnToMenu();

	/** Preferences submenu (mouse sensitivity + invert look), reachable from the
	 *  pause menu and the fallback text main menu; Back returns to the prior menu. */
	void ShowPreferences();

	TSharedRef<class SWidget> BuildMenu(const FText& Title, bool bPauseMenu);
	TSharedRef<class SWidget> BuildPreferencesMenu();
	TSharedRef<class SWidget> BuildMainMenuScreen();
	void PresentMenu(TSharedRef<class SWidget> Menu, bool bPause);

	TSharedPtr<class SWidget> MenuWidget;
	// Holds the authentic M1 main-menu bitmap (Coll_10 bitmap 001) for the Slate
	// SImage; must outlive the widget, so it lives on the controller.
	TSharedPtr<struct FSlateBrush> MenuBrush;
	// Chapter-splash widget + its texture brush (must outlive the widget).
	TSharedPtr<class SWidget> ChapterWidget;
	TSharedPtr<struct FSlateBrush> ChapterBrush;
	bool bMenuOpen = false;
	bool bChapterOpen = false;
	bool bOnMenuLevel = false;
};
