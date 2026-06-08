using UnrealBuildTool;
using System.Collections.Generic;

public class M1UE5Target : TargetRules
{
	public M1UE5Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("M1UE5");
	}
}
