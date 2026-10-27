#include "VoiceNTextSettings.h"

#include "HAL/PlatformMisc.h"

UVoiceNTextSettings::UVoiceNTextSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("VoiceNText");

	DefaultTranscription.InputFormat.SampleRate = 16000;
	DefaultTranscription.InputFormat.NumChannels = 1;
	DefaultTranscription.InputFormat.BitsPerSample = 16;

	SynthesizedAudioFormat.SampleRate = 24000;
	SynthesizedAudioFormat.NumChannels = 1;
	SynthesizedAudioFormat.BitsPerSample = 16;
}

FString UVoiceNTextSettings::ResolveApiKey() const
{
	if (!ApiKeyEnvVar.IsEmpty())
	{
		const FString FromEnv = FPlatformMisc::GetEnvironmentVariable(*ApiKeyEnvVar);
		if (!FromEnv.IsEmpty())
		{
			return FromEnv;
		}
	}

	return ApiKey;
}
