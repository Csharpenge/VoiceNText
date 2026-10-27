#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineTypes.h"
#include "VoiceNTextTypes.h"
#include "VoiceNText.h"
#include "VoiceNTextSubsystem.generated.h"

class UVoiceNTextSoundWave;
class UAudioComponent;

/**
 * Game-facing facade. One active backend instance, one transcription and one
 * synthesis at a time; every callback is delivered on the game thread.
 */
UCLASS()
class VOICENTEXT_API UVoiceNTextSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//~ Speech to text
	UFUNCTION(BlueprintCallable, Category = "VoiceNText|Speech To Text")
	bool StartTranscription(const FSpeechToTextRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "VoiceNText|Speech To Text")
	void PushAudio(const TArray<uint8>& PCM16);

	UFUNCTION(BlueprintCallable, Category = "VoiceNText|Speech To Text")
	void EndTranscription();

	UFUNCTION(BlueprintCallable, Category = "VoiceNText|Speech To Text")
	void CancelTranscription();

	UFUNCTION(BlueprintPure, Category = "VoiceNText|Speech To Text")
	bool IsTranscribing() const { return bTranscribing; }

	//~ Text to speech
	UFUNCTION(BlueprintCallable, Category = "VoiceNText|Text To Speech")
	bool StartSynthesis(const FTextToSpeechRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "VoiceNText|Text To Speech")
	void CancelSynthesis();

	UFUNCTION(BlueprintPure, Category = "VoiceNText|Text To Speech")
	bool IsSynthesizing() const { return bSynthesizing; }

	//~ Shared
	UFUNCTION(BlueprintCallable, Category = "VoiceNText")
	void CancelAll();

	UFUNCTION(BlueprintCallable, Category = "VoiceNText")
	void SetActiveProvider(FName InProviderId);

	UFUNCTION(BlueprintPure, Category = "VoiceNText")
	FName GetActiveProvider() const { return ActiveProviderId; }

	UPROPERTY(BlueprintAssignable, Category = "VoiceNText")
	FOnVoiceNTextTranscript OnTranscript;

	UPROPERTY(BlueprintAssignable, Category = "VoiceNText")
	FOnVoiceNTextAudioChunk OnAudioChunk;

	/** Fired when a task ends with Code == None. */
	UPROPERTY(BlueprintAssignable, Category = "VoiceNText")
	FOnVoiceNTextFinished OnFinished;

	/** Fired when a task fails; Code carries the reason. */
	UPROPERTY(BlueprintAssignable, Category = "VoiceNText")
	FOnVoiceNTextFailed OnFailed;

private:
	void EnsureProvider();
	void Fail(EVoiceNTextTask Task, EVoiceNTextErrorCode Code, const FString& Message, int32 HttpStatus = 0);
	void StopPlayback();
	UWorld* GetGameWorld() const;

	void HandleTranscript(const FTranscriptSegment& Segment);
	void HandleAudioChunk(const FAudioChunk& Chunk);
	void HandleProviderError(EVoiceNTextTask Task, const FVoiceNTextError& Error);
	void HandleTranscriptionFinished();
	void HandleSynthesisFinished();
	void CheckPlaybackFinished();

	TUniquePtr<IVoiceNTextProvider> Provider;
	FName ActiveProviderId;
	bool bTranscribing = false;
	bool bSynthesizing = false;

	UPROPERTY(Transient)
	TObjectPtr<UVoiceNTextSoundWave> ActiveWave;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveAudioComponent;

	FTimerHandle PlaybackTimerHandle;
};
