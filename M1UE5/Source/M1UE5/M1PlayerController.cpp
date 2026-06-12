#include "M1PlayerController.h"
#include "M1Character.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Engine/Texture2D.h"
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
	else
	{
		MaybeShowChapterScreen();
	}
}

void AM1PlayerController::MaybeShowChapterScreen()
{
	// Level index lives in the map name as "L_<NN>_..." (e.g. L_00_Arrival).
	const FString Map = GetWorld() ? GetWorld()->GetMapName() : FString();
	const int32 P = Map.Find(TEXT("L_"));
	if (P == INDEX_NONE)
	{
		return;
	}
	const FString Digits = Map.Mid(P + 2, 2);
	if (Digits.Len() < 2 || !Digits.IsNumeric())
	{
		return;
	}
	const int32 Idx = FCString::Atoi(*Digits);

	// Only six story-beat levels ship a chapter screen; LoadObject returns null
	// for the rest, which silently skips the splash.
	const FString TexPath =
		FString::Printf(TEXT("/Game/UI/Chapters/T_chapter_%02d.T_chapter_%02d"), Idx, Idx);
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *TexPath);
	if (!Tex)
	{
		return;
	}

	ChapterBrush = MakeShareable(new FSlateBrush());
	ChapterBrush->SetResourceObject(Tex);
	ChapterBrush->ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
	ChapterBrush->DrawAs = ESlateBrushDrawType::Image;

	// Full-screen click-catcher: invisible button over the whole viewport so a
	// click (or Enter/Space while focused) dismisses the splash.
	static const FButtonStyle ClearBtn = FButtonStyle()
		.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource())
		.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));

	TSharedRef<SButton> Splash = SNew(SButton).ButtonStyle(&ClearBtn)
		.OnClicked_Lambda([this]() { DismissChapterScreen(); return FReply::Handled(); })
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
			[
				SNew(SImage).Image(&GMenuBg).ColorAndOpacity(FLinearColor::Black)
			]
			+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
			[
				// 640x480 chapter art, aspect-locked + pillarboxed to any screen.
				SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SImage).Image(ChapterBrush.Get())
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 0.0f, 22.0f))
			[
				SNew(STextBlock).Text(LOCTEXT("Continue", "click to continue"))
				.Font(MenuFont(15)).ColorAndOpacity(FSlateColor(FLinearColor(0.30f, 1.0f, 0.40f, 0.85f)))
			]
		];

	ChapterWidget = Splash;
	if (GEngine && GEngine->GameViewport)
	{
		// ZOrder 200 -> above the HUD (which draws at default order).
		GEngine->GameViewport->AddViewportWidgetContent(Splash, 200);
	}
	bShowMouseCursor = true;
	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	FSlateApplication::Get().SetKeyboardFocus(Splash); // so Enter/Space also dismiss
	UGameplayStatics::SetGamePaused(this, true);
	bChapterOpen = true;
}

void AM1PlayerController::DismissChapterScreen()
{
	if (!bChapterOpen)
	{
		return;
	}
	if (GEngine && GEngine->GameViewport && ChapterWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ChapterWidget.ToSharedRef());
	}
	ChapterWidget.Reset();
	bChapterOpen = false;
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	UGameplayStatics::SetGamePaused(this, false);
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
	PresentMenu(BuildMainMenuScreen(), /*bPause*/ false);
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

TSharedRef<SWidget> AM1PlayerController::BuildMainMenuScreen()
{
	// Authentic M1 title screen (Shapes collection 10, bitmap 001). The option
	// labels (BEGIN NEW GAME ... QUIT) are baked into the bitmap; we lay invisible
	// click hotspots over the two options we actually implement.
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Menu/T_menu_main.T_menu_main"));
	if (!Tex)
	{
		// Never strand the player on a black screen -- fall back to the text menu.
		return BuildMenu(LOCTEXT("Title", "MARATHON"), /*bPause*/ false);
	}

	MenuBrush = MakeShareable(new FSlateBrush());
	MenuBrush->SetResourceObject(Tex);
	MenuBrush->ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
	MenuBrush->DrawAs = ESlateBrushDrawType::Image;

	// Invisible at rest, faint white wash on hover/press so the player can tell
	// the baked-in label under the cursor is live.
	static const FButtonStyle HotspotStyle = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateColorBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.16f)))
		.SetPressed(FSlateColorBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.28f)))
		.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));

	TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);
	auto AddHotspot = [&Canvas](float X0, float Y0, float X1, float Y1, TFunction<void()> Fn)
	{
		Canvas->AddSlot()
			.Anchors(FAnchors(X0, Y0, X1, Y1))
			.Offset(FMargin(0, 0, 0, 0))
			.Alignment(FVector2D(0, 0))
			[
				SNew(SButton).ButtonStyle(&HotspotStyle)
				.OnClicked_Lambda([Fn]() { Fn(); return FReply::Handled(); })
			];
	};

	// Normalized rects measured off the 435x325 bitmap.
	AddHotspot(0.345f, 0.085f, 0.793f, 0.175f, [this]() { OnNewGame(); });  // BEGIN NEW GAME
	AddHotspot(0.420f, 0.815f, 0.595f, 0.910f, [this]() { OnQuitGame(); }); // QUIT

	return SNew(SOverlay)
		// Black backdrop fills the pillarbox bars around the 4:3 bitmap.
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SNew(SImage).Image(&GMenuBg).ColorAndOpacity(FLinearColor::Black)
		]
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
			.HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				// Fixed 870x650 (=435x325 x2) design box keeps the hotspot anchors
				// aligned to the bitmap; SScaleBox fits it to any screen.
				SNew(SBox).WidthOverride(870.0f).HeightOverride(650.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[ SNew(SImage).Image(MenuBrush.Get()) ]
					+ SOverlay::Slot()[ Canvas ]
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(10.0f, 0.0f, 0.0f, 6.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("BUILD %s"), *AM1Character::GetBuildStamp())))
			.Font(MenuFont(11)).ColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.70f, 0.40f, 0.65f)))
		];
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
	Stack->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
	[
		SNew(STextBlock).Text(Title).Font(MenuFont(54)).ColorAndOpacity(FSlateColor(M1Green))
	];
	// Build identifier (compile timestamp) — proves a freshly packaged .exe is the
	// one actually running, vs a stale cached build. Auto-bumps on every recompile.
	Stack->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 0.0f, 0.0f, 30.0f))
	[
		SNew(STextBlock)
		.Text(FText::FromString(FString::Printf(TEXT("BUILD  %s"), *AM1Character::GetBuildStamp())))
		.Font(MenuFont(15)).ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.80f, 0.55f, 1.0f)))
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
