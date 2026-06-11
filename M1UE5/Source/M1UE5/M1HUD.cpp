#include "M1HUD.h"

#include "M1Character.h"
#include "M1WeaponComponent.h"
#include "M1WeaponSpriteTable.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

void AM1HUD::DrawHUD()
{
	Super::DrawHUD();

	AM1Character* Player = Cast<AM1Character>(GetOwningPawn());
	UM1WeaponComponent* Weapon = Player ? Player->GetWeaponComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!Weapon || !World || !Canvas)
	{
		return;
	}

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

	// Marathon's weapon art targets a 640x480 frame; scale with the viewport so
	// the gun fills a similar fraction of the screen, anchored bottom-center.
	const float Scale = (Canvas->SizeY / 480.0f) * 2.0f;
	const float W = Seq->PixelW * Scale;
	const float H = Seq->PixelH * Scale;
	DrawTexture(Tex, (Canvas->SizeX - W) * 0.5f, Canvas->SizeY - H,
		W, H, 0.0f, 0.0f, 1.0f, 1.0f);
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
