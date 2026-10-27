#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "VoiceNTextTypes.h"
#include "VoiceNTextSoundWave.generated.h"

/**
 * Procedural sound wave fed with PCM16 chunks as they arrive from the TTS backend.
 * Push from the game thread only (QueueAudio is safe, but the format must be set first).
 */
UCLASS()
class VOICENTEXT_API UVoiceNTextSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UVoiceNTextSoundWave(const FObjectInitializer& ObjectInitializer);

	/** Sets channels / sample rate. Call once before the first EnqueuePCM16. */
	void InitializeForFormat(const FAudioFormat& InFormat);

	void EnqueuePCM16(const TArray<uint8>& PCM16);

	/** No more chunks will arrive; playback ends once the queue drains. */
	void MarkFinished();

	bool IsPlaybackFinished() const;

	const FAudioFormat& GetFormat() const { return Format; }

private:
	UPROPERTY(Transient)
	FAudioFormat Format;

	bool bMarkedFinished = false;
};
