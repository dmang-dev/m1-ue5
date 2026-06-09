#include "M1PlayerController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "M1Menu"

namespace
{
	const FLinearColor M1Green(0.30f, 1.0f, 0.40f);
	const FSlateColorBrush GMenuBg(FLinearColor::White); // tinted per-use

	FSlateFontInfo MenuFont(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle("Bold", Size);
	}
}

void AM1PlayerController::BeginPlay()
{
	Super::BeginPlay();
	const FString Map = GetWorld() ? GetWorld()->GetMapName() : FString();
	bOnMenuLevel = Map.Contains(TEXT("MainMenu"));
	if (bOnMenuLevel)
	{
		ShowMainMenu();
	}
}

void AM1PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		FInputActionBinding& Bind =
			InputComponent->BindAction("Pause", IE_Pressed, this, &AM1PlayerController::TogglePauseMenu);
		Bind.bExecuteWhenPaused = true;
	}
}

void AM1PlayerController::ShowMainMenu()
{
	if (bMenuOpen)
	{
		return;
	}
	PresentMenu(BuildMenu(LOCTEXT("Title", "MARATHON"), /*bPause*/ false), /*bPause*/ false);
}

void AM1PlayerController::TogglePauseMenu()
{
	if (bOnMenuLevel)
	{
		return; // the main menu is already the screen here
	}
	if (bMenuOpen)
	{
		CloseMenu();
		return;
	}
	PresentMenu(BuildMenu(LOCTEXT("Paused", "PAUSED"), /*bPause*/ true), /*bPause*/ true);
}

void AM1PlayerController::PresentMenu(TSharedRef<SWidget> Menu, bool bPause)
{
	MenuWidget = Menu;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(Menu, 100);
	}
	bShowMouseCursor = true;
	// NB: do NOT SetWidgetToFocus(the SOverlay) — it isn't focusable and logs an
	// error; mouse input reaches the buttons fine without it.
	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	if (bPause)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}
	bMenuOpen = true;
}

void AM1PlayerController::CloseMenu()
{
	if (GEngine && GEngine->GameViewport && MenuWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
	}
	MenuWidget.Reset();
	bMenuOpen = false;
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	UGameplayStatics::SetGamePaused(this, false);
}

void AM1PlayerController::OnNewGame()
{
	// Tear the menu down BEFORE travelling — otherwise the widget lingers in the
	// viewport into the next level and its button lambdas dereference this (now
	// destroyed) controller on the next input event -> crash.
	CloseMenu();
	UGameplayStatics::OpenLevel(this, FName("/Game/Levels/M1/L_00_Arrival"));
}

void AM1PlayerController::OnQuitGame()
{
	CloseMenu();
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AM1PlayerController::OnReturnToMenu()
{
	CloseMenu();
	UGameplayStatics::OpenLevel(this, FName("/Game/Levels/M1/L_MainMenu"));
}

TSharedRef<SWidget> AM1PlayerController::BuildMenu(const FText& Title, bool bPauseMenu)
{
	auto Button = [this](const FText& Label, TFunction<void()> Fn) -> TSharedRef<SWidget>
	{
		return SNew(SBox).WidthOverride(380.0f).Padding(FMargin(0.0f, 7.0f))
		[
			SNew(SButton)
			.HAlign(HAlign_Center).VAlign(VAlign_Center)
			.ContentPadding(FMargin(18.0f, 12.0f))
			.OnClicked_Lambda([Fn]() { Fn(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(Label).Font(MenuFont(22))
				.ColorAndOpacity(FSlateColor(M1Green))
			]
		];
	};

	TSharedRef<SVerticalBox> Stack = SNew(SVerticalBox);
	Stack->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 0.0f, 0.0f, 36.0f))
	[
		SNew(STextBlock).Text(Title).Font(MenuFont(54)).ColorAndOpacity(FSlateColor(M1Green))
	];

	if (bPauseMenu)
	{
		Stack->AddSlot().AutoHeight().HAlign(HAlign_Center)[ Button(LOCTEXT("Resume", "Resume"), [this]() { CloseMenu(); }) ];
		Stack->AddSlot().AutoHeight().HAlign(HAlign_Center)[ Button(LOCTEXT("ToMenu", "Main Menu"), [this]() { OnReturnToMenu(); }) ];
		Stack->AddSlot().AutoHeight().HAlign(HAlign_Center)[ Button(LOCTEXT("QuitP", "Quit to Desktop"), [this]() { OnQuitGame(); }) ];
	}
	else
	{
		Stack->AddSlot().AutoHeight().HAlign(HAlign_Center)[ Button(LOCTEXT("New", "Begin New Game"), [this]() { OnNewGame(); }) ];
		Stack->AddSlot().AutoHeight().HAlign(HAlign_Center)[ Button(LOCTEXT("Prefs", "Preferences"), [this]() {}) ];
		Stack->AddSlot().AutoHeight().HAlign(HAlign_Center)[ Button(LOCTEXT("QuitM", "Quit"), [this]() { OnQuitGame(); }) ];
	}

	const FLinearColor BgTint = bPauseMenu ? FLinearColor(0.0f, 0.0f, 0.0f, 0.78f)
	                                       : FLinearColor(0.02f, 0.05f, 0.03f, 1.0f);

	return SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SNew(SImage).Image(&GMenuBg).ColorAndOpacity(BgTint)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			Stack
		];
}

#undef LOCTEXT_NAMESPACE
