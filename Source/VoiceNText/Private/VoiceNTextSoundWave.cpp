#include "VoiceNTextSoundWave.h"

#include "AudioDefines.h"

UVoiceNTextSoundWave::UVoiceNTextSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bLooping = false;
	NumChannels = 1;
	Duration = INDEFINITELY_LOOPING_DURATION;
	SoundGroup = SOUNDGROUP_Default;
	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
}

void UVoiceNTextSoundWave::InitializeForFormat(const FAudioFormat& InFormat)
{
	Format = InFormat;
	NumChannels = FMath::Max(1, Format.NumChannels);
	SetSampleRate(static_cast<uint32>(FMath::Clamp(Format.SampleRate, 8000, 192000)));
}

void UVoiceNTextSoundWave::EnqueuePCM16(const TArray<uint8>& PCM16)
{
	if (PCM16.Num() > 0)
	{
		QueueAudio(PCM16.GetData(), PCM16.Num());
	}
}

void UVoiceNTextSoundWave::MarkFinished()
{
	bMarkedFinished = true;
}

bool UVoiceNTextSoundWave::IsPlaybackFinished() const
{
	return bMarkedFinished && const_cast<UVoiceNTextSoundWave*>(this)->GetAvailableAudioByteCount() <= 0;
}
