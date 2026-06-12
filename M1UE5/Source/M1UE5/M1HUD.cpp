#include "M1HUD.h"

#include "M1Character.h"
#include "M1WeaponComponent.h"
#include "M1WeaponSpriteTable.h"
#include "M1HudTable.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"

namespace
{
	// --- Marathon interface layout (from Aleph One game_window.cpp research). ---
	//
	// The classic M1 HUD draws into the bottom strip of a 640x480 frame: the
	// interface bitmap is blitted to {0, 320, 640, 160}, i.e. the bottom third.
	// We reproduce that proportion: the panel art (Coll_10 bitmap 001, the main
	// 435x325 bezel) is anchored bottom-center and scaled to the viewport, the
	// same Canvas->SizeY/480 factor the viewmodel uses.
	//
	// The exact shield/oxygen/ammo pixel rects come from the original's
	// get_interface_rectangle() (which reads them out of the Shapes file's
	// interface data -- numbers we don't have as source constants). What we DO
	// have from AO: the ammo readouts all live on the RIGHT side of the panel
	// (screen x >= 385 of 640) and the status bars classically sit on the LEFT.
	// So we place the shield bar in the panel's lower-left and the ammo readout
	// in the lower-right. This is "reads as the Marathon HUD", not pixel-perfect,
	// and is expressed as fractions of the panel rect so it tracks any scale.

	constexpr float kPanelDesignH = 480.0f;  // Marathon art targets 640x480

	// Shield bar, as fractions of the drawn panel rect. Lower-left band.
	constexpr float kShieldX = 0.06f;   // left inset
	constexpr float kShieldY = 0.30f;   // top inset
	constexpr float kShieldW = 0.34f;   // full (100% shields) width
	constexpr float kShieldH = 0.10f;   // bar thickness

	// Oxygen bar sits just below the shield bar. Marathon depletes oxygen only
	// in vacuum; that system does not exist yet, so we draw it STATIC/FULL as a
	// placeholder so the panel reads correctly. (Omit by setting kDrawOxygen 0.)
	constexpr bool  kDrawOxygen = true;
	constexpr float kOxygenY = 0.44f;

	// Ammo readout anchor (icon + count), as fractions of the panel rect.
	// Lower-right, matching AO's right-side ammo positions.
	constexpr float kAmmoIconX = 0.62f;
	constexpr float kAmmoIconY = 0.30f;
	constexpr float kAmmoIconMaxH = 0.30f;  // icon height cap (frac of panel H)
	constexpr float kWpnIconX = 0.84f;      // weapon icon, far right
}

void AM1HUD::DrawHUD()
{
	Super::DrawHUD();

	// Never draw the gameplay HUD (panel, viewmodel, terminal) on the main menu,
	// even if the menu level falls back to the gameplay game mode.
	if (UWorld* W = GetWorld())
	{
		if (W->GetMapName().Contains(TEXT("MainMenu")))
		{
			return;
		}
	}

	AM1Character* Player = Cast<AM1Character>(GetOwningPawn());
	UM1WeaponComponent* Weapon = Player ? Player->GetWeaponComponent() : nullptr;
	if (!Player || !GetWorld() || !Canvas)
	{
		return;
	}

	// While reading a terminal, the lore page replaces the gun + panel entirely
	// (don't draw the weapon over the terminal).
	if (Player->IsTerminalOpen())
	{
		DrawTerminal(Player);
		return;
	}

	if (!Weapon)
	{
		return;
	}

	// Authentic 4:3 framed HUD: the world renders into the bezel's viewport hole
	// (SceneRT), the PICT 128 metal frame draws around it. Falls back to the
	// fullscreen view + bottom-strip HUD if the bezel art or RT isn't ready, so
	// the screen is never blank.
	UTexture2D* Bezel = GetBezelTexture();
	UTextureRenderTarget2D* RT = Player->GetSceneRT();
	if (Bezel && RT)
	{
		DrawFramedBezel(Player, Weapon, Bezel, RT);
	}
	else
	{
		DrawViewmodel(Weapon, 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);
		DrawInterfacePanel(Player, Weapon);
	}
}

void AM1HUD::DrawFramedBezel(AM1Character* Player, UM1WeaponComponent* Weapon,
	UTexture2D* Bezel, UTextureRenderTarget2D* RT)
{
	const float SX = Canvas->SizeX, SY = Canvas->SizeY;
	// Fit the 640x480 bezel by its limiting axis, centered (pillarbox on 16:9).
	const float Scale = FMath::Min(SX / 640.0f, SY / 480.0f);
	const float BW = 640.0f * Scale, BH = 480.0f * Scale;
	const float BX = (SX - BW) * 0.5f, BY = (SY - BH) * 0.5f;

	// Black behind everything (pillarbox bars + under the frame).
	DrawRect(FLinearColor::Black, 0.0f, 0.0f, SX, SY);

	// 3D-viewport hole (bezel px 174,21 .. 624,294 -> 450 x 273).
	const float HX = BX + 174.0f * Scale, HY = BY + 21.0f * Scale;
	const float HW = 450.0f * Scale, HH = 273.0f * Scale;

	// World view into the hole, then the gun over it.
	DrawTexture(RT, HX, HY, HW, HH, 0.0f, 0.0f, 1.0f, 1.0f);
	DrawViewmodel(Weapon, HX, HY, HW, HH);

	// Metal frame on top (its hole is transparent, so the world shows through).
	DrawTexture(Bezel, BX, BY, BW, BH, 0.0f, 0.0f, 1.0f, 1.0f);

	// Crosshair at the hole centre (where the camera looks).
	const float CX = HX + HW * 0.5f, CY = HY + HH * 0.5f;
	const FLinearColor Reticle(0.2f, 1.0f, 0.4f, 0.85f);
	DrawRect(Reticle, CX - 1.0f, CY - 7.0f, 2.0f, 14.0f);
	DrawRect(Reticle, CX - 7.0f, CY - 1.0f, 14.0f, 2.0f);

	DrawReadouts(Player, Weapon, BX, BY, Scale);
}

void AM1HUD::DrawReadouts(AM1Character* Player, UM1WeaponComponent* Weapon,
	float BX, float BY, float Scale)
{
	// Vertical meters (bezel px). Right column = shields (dynamic, fills bottom-up);
	// left column = oxygen (static full until the vacuum system exists).
	const float MeterTop = 126.0f, MeterH = 134.0f, MeterW = 28.0f;
	{
		const float Frac = FMath::Clamp(
			Player->GetHealth() / FMath::Max(1.0f, Player->GetMaxHealth()), 0.0f, 1.0f);
		const float X = BX + 122.0f * Scale, Y = BY + MeterTop * Scale;
		const float W = MeterW * Scale, H = MeterH * Scale;
		DrawRect(FLinearColor(0.0f, 0.05f, 0.10f, 0.9f), X, Y, W, H);
		DrawRect(FLinearColor(0.0f, 0.85f, 1.0f, 1.0f), X, Y + H * (1.0f - Frac), W, H * Frac);
	}
	{
		const float X = BX + 88.0f * Scale, Y = BY + MeterTop * Scale;
		const float W = MeterW * Scale, H = MeterH * Scale;
		DrawRect(FLinearColor(0.0f, 0.10f, 0.0f, 0.9f), X, Y, W, H);
		DrawRect(FLinearColor(0.0f, 0.85f, 0.20f, 1.0f), X, Y, W, H);
	}

	// Ammo readout in the message panel (bezel px ~x[238,615] y[386,448]).
	const int32 Slot = FMath::Clamp(Weapon->GetCurrentIndex(), 0,
		(int32)UE_ARRAY_COUNT(GM1HudIcons) - 1);
	const FM1HudIconRow& Icons = GM1HudIcons[Slot];
	const FM1WeaponDef& Def = Weapon->GetWeapon();
	float CX = BX + 238.0f * Scale;
	const float CY = BY + 386.0f * Scale;
	const float IconH = 46.0f * Scale;
	if (UTexture2D* WpnTex = GetHudTexture(Icons.Weapon))
	{
		const float S = IconH / FMath::Max(1, WpnTex->GetSizeY());
		const float W = WpnTex->GetSizeX() * S;
		DrawTexture(WpnTex, CX, CY, W, IconH, 0.0f, 0.0f, 1.0f, 1.0f);
		CX += W + 12.0f * Scale;
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (Font && Def.MagSize > 0)
	{
		const FString AmmoStr = FString::Printf(TEXT("%d / %d"), Weapon->GetAmmo(), Def.MagSize);
		DrawText(AmmoStr, FLinearColor(0.6f, 1.0f, 0.7f, 1.0f),
			CX, CY + IconH * 0.2f, Font, FMath::Max(1.0f, Scale * 1.6f));
	}
}

UTexture2D* AM1HUD::GetBezelTexture()
{
	const FString Key = TEXT("__bezel");
	if (UTexture2D** Found = FrameCache.Find(Key))
	{
		return *Found;
	}
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/UI/HUD/T_hud_bezel.T_hud_bezel"));
	FrameCache.Add(Key, Tex);
	return Tex;
}

void AM1HUD::DrawTerminal(AM1Character* Player)
{
	// Full-screen dark backdrop so the room behind doesn't bleed through.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f), 0.0f, 0.0f,
		Canvas->SizeX, Canvas->SizeY);

	const int32 TermId = Player->GetTerminalTextureId();
	const int32 Page = Player->GetTerminalPage();

	// Page art: centered, scaled to fit the viewport height (renders are ~640
	// wide). Null-guard: if the texture isn't imported yet, just show the hint.
	UTexture2D* Tex = GetTerminalTexture(TermId, Page);
	if (Tex)
	{
		const int32 TexW = FMath::Max(1, Tex->GetSizeX());
		const int32 TexH = FMath::Max(1, Tex->GetSizeY());
		// Fit to ~90% of viewport height, never upscaling past the width either.
		const float ScaleH = (Canvas->SizeY * 0.9f) / TexH;
		const float ScaleW = (Canvas->SizeX * 0.95f) / TexW;
		const float Scale = FMath::Min(ScaleH, ScaleW);
		const float W = TexW * Scale;
		const float H = TexH * Scale;
		DrawTexture(Tex, (Canvas->SizeX - W) * 0.5f, (Canvas->SizeY - H) * 0.5f,
			W, H, 0.0f, 0.0f, 1.0f, 1.0f);
	}

	// Paging hint, terminal green, bottom-center: "PAGE n/N  -  [E] continue".
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (Font)
	{
		const int32 Pages = FMath::Max(1, Player->GetTerminalPageCount());
		const FString Hint = FString::Printf(
			TEXT("PAGE %d/%d   -   [E] continue"), Page + 1, Pages);
		const float TextScale = FMath::Max(1.0f, (Canvas->SizeY / 480.0f) * 1.2f);
		DrawText(Hint, FLinearColor(0.2f, 1.0f, 0.2f, 1.0f),
			Canvas->SizeX * 0.5f - 120.0f * TextScale, Canvas->SizeY * 0.94f,
			Font, TextScale);
	}
}

void AM1HUD::DrawViewmodel(UM1WeaponComponent* Weapon, float RX, float RY, float RW, float RH)
{
	UWorld* World = GetWorld();
	const int32 Index = Weapon->GetCurrentIndex();
	if (Index < 0 || Index >= (int32)UE_ARRAY_COUNT(GM1WeaponSprites))
	{
		return;
	}
	const FM1WeaponSpriteRow& Row = GM1WeaponSprites[Index];

	// Pick the state sequence: reload > recent shot > idle. Sequences without
	// frames (e.g. the Rocket Launcher has no canonical firing shape) fall back
	// to idle, matching the data rather than inventing art.
	const float Now = World->GetTimeSeconds();
	const FM1WeaponSpriteSeq* Seq = &Row.Idle;
	float SeqTime = Now;
	if (Weapon->IsReloading() && Row.Reload.Base)
	{
		Seq = &Row.Reload;
	}
	else if (Row.Fire.Base && Row.Fire.Fps > 0.0f)
	{
		const float SinceShot = Now - Weapon->GetLastFireTime();
		if (SinceShot >= 0.0f && SinceShot < Row.Fire.Frames / Row.Fire.Fps)
		{
			Seq = &Row.Fire;
			SeqTime = SinceShot;  // play the fire anim once from the shot
		}
	}
	if (!Seq->Base || Seq->Frames <= 0)
	{
		return;
	}

	const int32 Frame = (Seq == &Row.Fire)
		? FMath::Clamp((int32)(SeqTime * Seq->Fps), 0, Seq->Frames - 1)
		: ((int32)(SeqTime * Seq->Fps) % Seq->Frames);

	UTexture2D* Tex = GetFrameTexture(Seq->Base, Frame);
	if (!Tex)
	{
		return;
	}

	// Marathon's weapon art targets a 640x480 frame; scale with the draw rect so
	// the gun fills a similar fraction, anchored bottom-center of the rect (the
	// bezel hole when framed, else the full screen).
	const float Scale = (RH / kPanelDesignH) * 2.0f;
	const float W = Seq->PixelW * Scale;
	const float H = Seq->PixelH * Scale;
	DrawTexture(Tex, RX + (RW - W) * 0.5f, RY + RH - H,
		W, H, 0.0f, 0.0f, 1.0f, 1.0f);
}

void AM1HUD::DrawInterfacePanel(AM1Character* Player, UM1WeaponComponent* Weapon)
{
	// --- Panel chrome: bottom-center, scaled to the viewport like the viewmodel.
	//     If the texture is absent (not yet rendered/imported) we skip it but
	//     still draw the bars/ammo at the placement they WOULD occupy, so the
	//     HUD is informative even before the art lands. ---
	// IMPORTANT: Marathon's in-game status-bar art is NOT in our extracted Shapes.
	// The "interface" collection we have (Coll_10) is the TITLE/MENU screens --
	// "T_hud_panel" (Coll_10 bitmap 001) is literally the main-menu bitmap, so
	// drawing it as the HUD panel slapped the whole menu over gameplay. Until the
	// real in-game bezel is sourced, draw NO panel chrome: the shields + ammo lay
	// out against an invisible bottom strip (AO's {0,320,640,160} interface blit
	// proportion), anchored to the screen edges.
	const float ViewScale = Canvas->SizeY / kPanelDesignH;
	const float PanelW = Canvas->SizeX;
	const float PanelH = Canvas->SizeY * (160.0f / 480.0f);
	const float PanelX = 0.0f;
	const float PanelY = Canvas->SizeY - PanelH;

	// --- Shield bar. width = (health/max) * full, fill left-to-right.
	//     Marathon shield color is cyan. DrawRect a dim back-rail then the fill,
	//     so an empty bar still reads as "the shield region". ---
	if (Player)
	{
		const float MaxHealth = FMath::Max(1.0f, Player->GetMaxHealth());
		const float Frac = FMath::Clamp(Player->GetHealth() / MaxHealth, 0.0f, 1.0f);

		const float BarX = PanelX + kShieldX * PanelW;
		const float BarY = PanelY + kShieldY * PanelH;
		const float BarFullW = kShieldW * PanelW;
		const float BarH = kShieldH * PanelH;

		// Back rail (dark blue) then the cyan fill scaled by shield fraction.
		DrawRect(FLinearColor(0.0f, 0.06f, 0.12f, 0.85f), BarX, BarY, BarFullW, BarH);
		DrawRect(FLinearColor(0.0f, 0.75f, 1.0f, 1.0f), BarX, BarY, BarFullW * Frac, BarH);

		// Oxygen placeholder: full/static green bar (no vacuum system yet). It
		// shares the shield bar's X/width so the two stack like the original.
		if (kDrawOxygen)
		{
			const float OxyY = PanelY + kOxygenY * PanelH;
			DrawRect(FLinearColor(0.0f, 0.10f, 0.0f, 0.85f), BarX, OxyY, BarFullW, BarH);
			DrawRect(FLinearColor(0.0f, 0.85f, 0.20f, 1.0f), BarX, OxyY, BarFullW, BarH);
		}
	}

	// --- Ammo readout: weapon icon + ammo icon + count, lower-right. ---
	const int32 Slot = Weapon->GetCurrentIndex();
	if (Slot < 0 || Slot >= (int32)UE_ARRAY_COUNT(GM1HudIcons))
	{
		return;
	}
	const FM1HudIconRow& Icons = GM1HudIcons[Slot];

	const float IconMaxH = kAmmoIconMaxH * PanelH;

	// Weapon icon (far right). Skip the Fist (no inventory art).
	if (UTexture2D* WpnTex = GetHudTexture(Icons.Weapon))
	{
		const float Scale = IconMaxH / FMath::Max(1, WpnTex->GetSizeY());
		const float W = WpnTex->GetSizeX() * Scale;
		const float H = WpnTex->GetSizeY() * Scale;
		DrawTexture(WpnTex, PanelX + kWpnIconX * PanelW, PanelY + kAmmoIconY * PanelH,
			W, H, 0.0f, 0.0f, 1.0f, 1.0f);
	}

	// Ammo icon + count. The Fist (slot 0) and the depleting Alien weapon have
	// no ammo icon -> skip the icon but still show the count for finite mags.
	const FM1WeaponDef& Def = Weapon->GetWeapon();
	const int32 Ammo = Weapon->GetAmmo();

	float TextX = PanelX + kAmmoIconX * PanelW;
	const float TextY = PanelY + kAmmoIconY * PanelH;
	if (UTexture2D* AmmoTex = GetHudTexture(Icons.Ammo))
	{
		const float Scale = IconMaxH / FMath::Max(1, AmmoTex->GetSizeY());
		const float W = AmmoTex->GetSizeX() * Scale;
		const float H = AmmoTex->GetSizeY() * Scale;
		DrawTexture(AmmoTex, TextX, TextY, W, H, 0.0f, 0.0f, 1.0f, 1.0f);
		TextX += W + 6.0f * ViewScale;  // count sits to the right of the icon
	}

	// Only show a number for weapons that actually track ammo: finite mag, not
	// the Fist's infinite sentinel. Magnum/AR/etc. show "cur / mag".
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (Font && Def.MagSize > 0)
	{
		const FString AmmoStr = FString::Printf(TEXT("%d / %d"), Ammo, Def.MagSize);
		// Scale the text up at high res so it stays readable at 1080p+.
		const float TextScale = FMath::Max(1.0f, ViewScale * 1.5f);
		DrawText(AmmoStr, FLinearColor(0.9f, 0.95f, 1.0f, 1.0f),
			TextX, TextY + IconMaxH * 0.25f, Font, TextScale);

		// AR grenades (secondary mag): show "GR cur / mag" with its icon below.
		const int32 SecAmmo = Weapon->GetSecAmmo();
		if (SecAmmo >= 0 && Def.SecMagSize > 0)
		{
			const float SecY = TextY + IconMaxH * 0.55f;
			float SecX = PanelX + kAmmoIconX * PanelW;
			if (UTexture2D* SecTex = GetHudTexture(Icons.SecAmmo))
			{
				const float Scale = (IconMaxH * 0.5f) / FMath::Max(1, SecTex->GetSizeY());
				const float W = SecTex->GetSizeX() * Scale;
				const float H = SecTex->GetSizeY() * Scale;
				DrawTexture(SecTex, SecX, SecY, W, H, 0.0f, 0.0f, 1.0f, 1.0f);
				SecX += W + 6.0f * ViewScale;
			}
			const FString SecStr = FString::Printf(TEXT("%d / %d"), SecAmmo, Def.SecMagSize);
			DrawText(SecStr, FLinearColor(1.0f, 0.85f, 0.3f, 1.0f),
				SecX, SecY, Font, FMath::Max(1.0f, ViewScale * 1.2f));
		}
	}
}

UTexture2D* AM1HUD::GetFrameTexture(const FString& AssetBase, int32 Frame)
{
	const FString Key = FString::Printf(TEXT("%s%d"), *AssetBase, Frame);
	if (UTexture2D** Found = FrameCache.Find(Key))
	{
		return *Found;
	}
	const FString Path = FString::Printf(
		TEXT("/Game/Sprites/Weapons/%s.%s"), *Key, *Key);
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path);
	FrameCache.Add(Key, Tex);  // cache nulls too: one failed load, not 60/s
	return Tex;
}

UTexture2D* AM1HUD::GetTerminalTexture(int32 TermId, int32 Page)
{
	const FString Key = FString::Printf(TEXT("term_%d_%d"), TermId, Page);
	if (UTexture2D** Found = FrameCache.Find(Key))
	{
		return *Found;
	}
	// The page asset is T_term<id>_p<page> under /Game/UI/Terminals (imported by
	// ue5_import_terminals.py). Missing until the orchestrator runs that import.
	const FString Leaf = FString::Printf(TEXT("T_term%d_p%d"), TermId, Page);
	const FString Path = FString::Printf(
		TEXT("/Game/UI/Terminals/%s.%s"), *Leaf, *Leaf);
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path);
	FrameCache.Add(Key, Tex);  // cache nulls too (art may not exist yet)
	return Tex;
}

UTexture2D* AM1HUD::GetHudTexture(const TCHAR* AssetName)
{
	if (!AssetName || AssetName[0] == TEXT('\0'))
	{
		return nullptr;  // nullptr entry in the slot table (e.g. Fist has no icon)
	}
	const FString Key = AssetName;
	if (UTexture2D** Found = FrameCache.Find(Key))
	{
		return *Found;
	}
	const FString Path = FString::Printf(
		TEXT("/Game/Sprites/HUD/%s.%s"), AssetName, AssetName);
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path);
	FrameCache.Add(Key, Tex);  // cache nulls too (art may not exist yet)
	return Tex;
}
