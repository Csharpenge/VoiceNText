#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioCaptureCore.h"
#include "VoiceNTextTypes.h"
#include "VoiceNTextCaptureComponent.generated.h"

/**
 * Microphone capture. Converts the device format (usually 48 kHz float) into the
 * requested PCM16 format and either broadcasts chunks or feeds the subsystem.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class VOICENTEXT_API UVoiceNTextCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVoiceNTextCaptureComponent();

	UFUNCTION(BlueprintCallable, Category = "VoiceNText|Capture")
	bool StartCapture();

	UFUNCTION(BlueprintCallable, Category = "VoiceNText|Capture")
	void StopCapture();

	UFUNCTION(BlueprintPure, Category = "VoiceNText|Capture")
	bool IsCapturing() const { return bCapturing; }

	/** Format pushed to listeners; default 16 kHz / mono / 16 bit (typical ASR input). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText|Capture")
	FAudioFormat OutputFormat;

	/** When true, chunks are also pushed to UVoiceNTextSubsystem::PushAudio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText|Capture")
	bool bForwardToSubsystem = true;

	UPROPERTY(BlueprintAssignable, Category = "VoiceNText|Capture")
	FOnVoiceNTextAudioChunk OnAudioChunk;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnDeviceAudio(const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverflow);
	void DrainPendingChunks();

	TUniquePtr<Audio::FAudioCapture> Capture;
	TQueue<FAudioChunk, EQueueMode::Spsc> PendingChunks;
	bool bCapturing = false;
};
