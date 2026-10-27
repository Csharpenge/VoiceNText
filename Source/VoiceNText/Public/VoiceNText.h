#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "VoiceNTextTypes.h"

using FVoiceNTextTranscriptCallback = TFunction<void(const FTranscriptSegment&)>;
using FVoiceNTextAudioCallback = TFunction<void(const FAudioChunk&)>;
using FVoiceNTextErrorCallback = TFunction<void(const FVoiceNTextError&)>;
using FVoiceNTextFinishedCallback = TFunction<void()>;

/**
 * Backend contract. Implementations are registered with FVoiceNTextModule and
 * owned by UVoiceNTextSubsystem, so callbacks always arrive on the game thread.
 */
class IVoiceNTextProvider
{
public:
	virtual ~IVoiceNTextProvider() = default;

	virtual FName GetId() const = 0;

	/** Cheap preflight check (missing URL, missing token, ...). */
	virtual bool IsAvailable(FString& OutReason) const = 0;

	//~ Speech to text
	virtual bool BeginTranscription(
		const FSpeechToTextRequest& Request,
		FVoiceNTextTranscriptCallback OnTranscript,
		FVoiceNTextErrorCallback OnError,
		FVoiceNTextFinishedCallback OnFinished) = 0;

	/** Feed PCM16 mono audio in the format declared by the request. */
	virtual void PushAudio(const TArray<uint8>& PCM16) {}
	virtual void EndTranscription() {}
	virtual void CancelTranscription() {}

	//~ Text to speech
	virtual bool BeginSynthesis(
		const FTextToSpeechRequest& Request,
		FVoiceNTextAudioCallback OnAudio,
		FVoiceNTextErrorCallback OnError,
		FVoiceNTextFinishedCallback OnFinished) = 0;

	virtual void CancelSynthesis() {}

	virtual void Shutdown() {}
};

/** Runtime module: owns the backend registry. */
class VOICENTEXT_API FVoiceNTextModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FVoiceNTextModule& Get();

	void RegisterProvider(FName Id, TFunction<TUniquePtr<IVoiceNTextProvider>()> Factory);
	void UnregisterProvider(FName Id);
	TUniquePtr<IVoiceNTextProvider> CreateProvider(FName Id) const;
	TArray<FName> GetProviderIds() const;

	bool IsValidProvider(FName Id) const { return Factories.Contains(Id); }

private:
	TMap<FName, TFunction<TUniquePtr<IVoiceNTextProvider>()>> Factories;
};
