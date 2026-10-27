using UnrealBuildTool;

public class VoiceNText : ModuleRules
{
	public VoiceNText(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AudioMixerCore",
			"AudioCaptureCore",
			"DeveloperSettings",
			"HTTP",
			"Json"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AudioMixer",
			"AudioExtensions"
		});
	}
}
