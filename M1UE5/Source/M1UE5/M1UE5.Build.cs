using UnrealBuildTool;

public class M1UE5 : ModuleRules
{
	public M1UE5(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			// Runtime asset loader (installer Phase 0): build levels from OBJ/PNG at
			// runtime with no cook. ProceduralMeshComponent = geometry sections;
			// ImageWrapper = PNG decode; RenderCore/RHI = CreateTransient textures.
			"ProceduralMeshComponent",
			"ImageWrapper",
			"RenderCore",
			"RHI",
			"Json"   // parse <stem>_placements.json (player start) at runtime (Phase 1)
		});
	}
}
