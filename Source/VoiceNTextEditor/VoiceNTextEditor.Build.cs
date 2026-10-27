using UnrealBuildTool;

public class VoiceNTextEditor : ModuleRules
{
	public VoiceNTextEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"PropertyEditor",
			"InputCore",
			"VoiceNText"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
