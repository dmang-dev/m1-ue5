#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "M1HUD.generated.h"

class UTexture2D;

/**
 * Marathon weapon-in-hand viewmodel, drawn as a HUD sprite (the original
 * renders the weapon as a screen-space sprite, not a 3D model). Frames come
 * from the generated M1WeaponSpriteTable.h (Shapes.shps collection 1, selected
 * by the canonical Physics.phys idle/firing/reloading shape indices) and are
 * picked each frame from the weapon component's state.
 */
UCLASS()
class M1UE5_API AM1HUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	UTexture2D* GetFrameTexture(const FString& AssetBase, int32 Frame);

	/** Runtime-loaded frame textures, cached + rooted so GC can't drop them
	 *  mid-session (they're loaded by string path, invisible to the cooker's
	 *  reference walk -- the always-cook rule for /Game/Sprites covers them). */
	UPROPERTY()
	TMap<FString, UTexture2D*> FrameCache;
};
