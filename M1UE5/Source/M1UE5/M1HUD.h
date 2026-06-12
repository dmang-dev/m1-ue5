#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "M1HUD.generated.h"

class AM1Character;
class UM1WeaponComponent;
class UTexture2D;

/**
 * Marathon HUD. Draws (in order) the weapon-in-hand viewmodel sprite, then the
 * Marathon interface panel chrome along the bottom strip, the shield bar, and
 * the current weapon's ammo readout (icon + count). The viewmodel frames come
 * from the generated M1WeaponSpriteTable.h (Shapes.shps collection 1); the
 * panel/icon art comes from the generated M1HudTable.h (Coll_10 chrome +
 * Coll_07 icons, imported to /Game/Sprites/HUD by the gen pipeline).
 *
 * Every texture loads by string path at runtime and is cached + null-guarded:
 * the HUD art does not exist until the orchestrator renders+imports it, so a
 * missing texture just skips that element rather than crashing.
 */
UCLASS()
class M1UE5_API AM1HUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	/** Weapon-in-hand viewmodel (bottom-center). Unchanged from the original. */
	void DrawViewmodel(UM1WeaponComponent* Weapon);

	/** Marathon interface panel: bottom-strip chrome + shield bar + ammo. */
	void DrawInterfacePanel(AM1Character* Player, UM1WeaponComponent* Weapon);

	/** Terminal (lore) overlay: dark backdrop + the active green-on-black page
	 *  art, centered + scaled to the viewport, with a paging hint. Drawn instead
	 *  of the viewmodel/panel while the player is reading a terminal. */
	void DrawTerminal(AM1Character* Player);

	/** Terminal page loader: /Game/UI/Terminals/T_term<id>_p<page>. Cached. */
	UTexture2D* GetTerminalTexture(int32 TermId, int32 Page);

	/** Viewmodel frame loader: /Game/Sprites/Weapons/<AssetBase><Frame>. */
	UTexture2D* GetFrameTexture(const FString& AssetBase, int32 Frame);

	/** HUD-art loader by leaf asset name under /Game/Sprites/HUD. Caches nulls
	 *  too (one failed load, not one per frame). Returns nullptr if absent. */
	UTexture2D* GetHudTexture(const TCHAR* AssetName);

	/** Runtime-loaded textures, cached + rooted so GC can't drop them mid-session
	 *  (they're loaded by string path, invisible to the cooker's reference walk
	 *  -- the always-cook rule for /Game/Sprites covers them). */
	UPROPERTY()
	TMap<FString, UTexture2D*> FrameCache;
};
