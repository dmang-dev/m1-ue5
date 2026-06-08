using UnrealBuildTool;
using System.Collections.Generic;

public class M1UE5EditorTarget : TargetRules
{
	public M1UE5EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("M1UE5");
	}
}
