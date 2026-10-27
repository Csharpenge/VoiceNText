#pragma once

#include "CoreMinimal.h"
#include "VoiceNText.h"

/** Placeholder backend: exposes the pipeline but refuses to run. */
class FVoiceNTextNullProvider : public IVoiceNTextProvider
{
public:
	virtual FName GetId() const override { return TEXT("Null"); }

	virtual bool IsAvailable(FString& OutReason) const override
	{
		OutReason = TEXT("VoiceNText: ProviderId is 'Null'. Switch ProviderId to 'Rest' and fill TranscribeUrl / SynthesizeUrl in Project Settings > Plugins > Voice N Text.");
		return false;
	}

	virtual bool BeginTranscription(const FSpeechToTextRequest&, FVoiceNTextTranscriptCallback, FVoiceNTextErrorCallback, FVoiceNTextFinishedCallback) override
	{
		return false;
	}

	virtual bool BeginSynthesis(const FTextToSpeechRequest&, FVoiceNTextAudioCallback, FVoiceNTextErrorCallback, FVoiceNTextFinishedCallback) override
	{
		return false;
	}
};
