#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M1RuntimeLevel.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UTexture2D;

/**
 * Installer Phase 0: builds a Marathon level's geometry AT RUNTIME from the
 * converted intermediates (OBJ + MTL + PNG) on disk -- no cooked .uasset / .umap.
 * One UProceduralMeshComponent, one mesh section per OBJ `usemtl`, each section
 * an instance of the committed unlit master material /Game/Runtime/M_M1Runtime
 * with its texture + per-surface emissive bucket (the `_b<pct>` in the material
 * name, matching the cooked pipeline). Coordinates mirror the editor OBJ import
 * (negate Y + reverse winding). This is the core of the bring-your-own-assets
 * runtime architecture (docs/INSTALLER_DESIGN.md, Option c).
 */
UCLASS()
class M1UE5_API AM1RuntimeLevel : public AActor
{
	GENERATED_BODY()

public:
	AM1RuntimeLevel();

	/** Resolve the data root: env M1_DATA_ROOT, else <ProjectDir>/Reference, else
	 *  the dev source tree. (Phase 2 points this at the per-user AppData cache.) */
	static FString ResolveDataRoot();

	/** Build <DataRoot>/Geometry/<Stem>.obj into this actor's mesh. Returns the
	 *  number of mesh sections created (0 = failed). Logs a [M1Runtime] summary. */
	int32 BuildFromStem(const FString& Stem);

	/** Read <DataRoot>/Geometry/<Stem>_placements.json "player_start" into OutLoc
	 *  (UE cm, the SAME final space as the runtime geometry -- placements already
	 *  match the editor-imported mesh) + OutYaw (deg). False if missing. */
	static bool LoadPlayerStart(const FString& Stem, FVector& OutLoc, float& OutYaw);

	/** Sections built + total vertices/triangles + world bounds extent (for the
	 *  self-test). Valid after BuildFromStem. */
	int32 NumSections = 0;
	int32 TotalVerts = 0;
	int32 TotalTris = 0;
	FVector BoundsExtent = FVector::ZeroVector;

private:
	UPROPERTY(VisibleAnywhere)
	UProceduralMeshComponent* Mesh = nullptr;

	UPROPERTY()
	UMaterialInterface* MasterMaterial = nullptr;

	/** Decode a PNG file on disk to a transient sRGB nearest-filtered texture. */
	UTexture2D* LoadPNG(const FString& AbsPath);

	/** newmtl name -> absolute texture PNG path, parsed from the .mtl `map_Kd`. */
	TMap<FString, FString> ParseMTL(const FString& MtlPath);

	/** Resolve a level stem to its actual Reference/Geometry base filename. The
	 *  cooked .umap name and the OBJ/placements filename sanitize the M1 level name
	 *  differently (e.g. "Low_Flying" vs "Low-Flying", trailing "_"), so we match by
	 *  the reliable leading "NN_" index prefix when the exact stem isn't present. */
	static FString ResolveGeoBasename(const FString& Stem);

	/** Cache: absolute PNG path -> texture (one decode per unique image). */
	UPROPERTY()
	TMap<FString, UTexture2D*> TextureCache;
};
