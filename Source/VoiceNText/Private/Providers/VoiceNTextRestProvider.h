#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "VoiceNText.h"

/**
 * Generic REST backend:
 *   POST TranscribeUrl  (body: WAV) -> JSON {"text": "..."}
 *   POST SynthesizeUrl  (body: JSON) -> WAV or raw PCM16
 *
 * The interface is stream shaped; this implementation delivers one chunk per
 * response. Vendor protocols (WebSocket / SSE) plug in by subclassing or by
 * registering another IVoiceNTextProvider.
 */
class FVoiceNTextRestProvider : public IVoiceNTextProvider
{
public:
	FVoiceNTextRestProvider();
	virtual ~FVoiceNTextRestProvider() override;

	virtual FName GetId() const override { return TEXT("Rest"); }
	virtual bool IsAvailable(FString& OutReason) const override;

	virtual bool BeginTranscription(
		const FSpeechToTextRequest& Request,
		FVoiceNTextTranscriptCallback OnTranscript,
		FVoiceNTextErrorCallback OnError,
		FVoiceNTextFinishedCallback OnFinished) override;

	virtual void PushAudio(const TArray<uint8>& PCM16) override;
	virtual void EndTranscription() override;
	virtual void CancelTranscription() override;

	virtual bool BeginSynthesis(
		const FTextToSpeechRequest& Request,
		FVoiceNTextAudioCallback OnAudio,
		FVoiceNTextErrorCallback OnError,
		FVoiceNTextFinishedCallback OnFinished) override;

	virtual void CancelSynthesis() override;
	virtual void Shutdown() override;

private:
	void HandleTranscribeResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected);
	void HandleSynthesizeResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected);
	void ResetCallbacks();
	FVoiceNTextError MakeError(FHttpResponsePtr Response, bool bConnected, const TCHAR* Context) const;
	void EmitError(const FVoiceNTextError& Error);
	void EmitFinished();

	FSpeechToTextRequest CurrentTranscriptionRequest;
	FTextToSpeechRequest CurrentSynthesisRequest;

	FVoiceNTextTranscriptCallback TranscriptCallback;
	FVoiceNTextAudioCallback AudioCallback;
	FVoiceNTextErrorCallback ErrorCallback;
	FVoiceNTextFinishedCallback FinishedCallback;

	TArray<uint8> PendingPCM16;

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> TranscribeRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> SynthesizeRequest;

	bool bShuttingDown = false;
};
