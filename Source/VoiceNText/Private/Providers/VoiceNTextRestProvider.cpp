#include "Providers/VoiceNTextRestProvider.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VoiceNTextPcm.h"
#include "VoiceNTextSettings.h"

FVoiceNTextRestProvider::FVoiceNTextRestProvider() = default;

FVoiceNTextRestProvider::~FVoiceNTextRestProvider()
{
	Shutdown();
}

bool FVoiceNTextRestProvider::IsAvailable(FString& OutReason) const
{
	const UVoiceNTextSettings* Settings = UVoiceNTextSettings::Get();
	if (Settings->TranscribeUrl.IsEmpty() && Settings->SynthesizeUrl.IsEmpty())
	{
		OutReason = TEXT("VoiceNText: ProviderId is 'Rest' but neither TranscribeUrl nor SynthesizeUrl is set (Project Settings > Plugins > Voice N Text).");
		return false;
	}

	return true;
}

bool FVoiceNTextRestProvider::BeginTranscription(
	const FSpeechToTextRequest& Request,
	FVoiceNTextTranscriptCallback OnTranscript,
	FVoiceNTextErrorCallback OnError,
	FVoiceNTextFinishedCallback OnFinished)
{
	const UVoiceNTextSettings* Settings = UVoiceNTextSettings::Get();
	if (Settings->TranscribeUrl.IsEmpty())
	{
		return false;
	}

	TranscriptCallback = MoveTemp(OnTranscript);
	ErrorCallback = MoveTemp(OnError);
	FinishedCallback = MoveTemp(OnFinished);
	CurrentTranscriptionRequest = Request;
	PendingPCM16.Reset();
	return true;
}

void FVoiceNTextRestProvider::PushAudio(const TArray<uint8>& PCM16)
{
	PendingPCM16.Append(PCM16);
}

void FVoiceNTextRestProvider::EndTranscription()
{
	if (bShuttingDown)
	{
		return;
	}

	if (PendingPCM16.Num() == 0)
	{
		FVoiceNTextError Error;
		Error.Code = EVoiceNTextErrorCode::InvalidRequest;
		Error.Message = TEXT("VoiceNText: no audio was pushed before EndTranscription().");
		EmitError(Error);
		EmitFinished();
		return;
	}

	const UVoiceNTextSettings* Settings = UVoiceNTextSettings::Get();

	TArray<uint8> WavBytes;
	VoiceNText::Pcm::WrapPcm16AsWav(PendingPCM16, CurrentTranscriptionRequest.InputFormat, WavBytes);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Settings->TranscribeUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("audio/wav"));
	Request->SetTimeout(60.f);

	const FString ApiKey = Settings->ResolveApiKey();
	if (!ApiKey.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	}
	if (!Settings->ModelName.IsEmpty())
	{
		Request->SetHeader(TEXT("X-VoiceNText-Model"), Settings->ModelName);
	}

	Request->SetContent(WavBytes);
	Request->OnProcessRequestComplete().BindRaw(this, &FVoiceNTextRestProvider::HandleTranscribeResponse);

	TranscribeRequest = Request;
	Request->ProcessRequest();
}

void FVoiceNTextRestProvider::CancelTranscription()
{
	if (TranscribeRequest.IsValid())
	{
		TranscribeRequest->CancelRequest();
		TranscribeRequest.Reset();
	}
	PendingPCM16.Reset();
}

bool FVoiceNTextRestProvider::BeginSynthesis(
	const FTextToSpeechRequest& Request,
	FVoiceNTextAudioCallback OnAudio,
	FVoiceNTextErrorCallback OnError,
	FVoiceNTextFinishedCallback OnFinished)
{
	const UVoiceNTextSettings* Settings = UVoiceNTextSettings::Get();
	if (Settings->SynthesizeUrl.IsEmpty() || Request.Text.IsEmpty())
	{
		return false;
	}

	AudioCallback = MoveTemp(OnAudio);
	ErrorCallback = MoveTemp(OnError);
	FinishedCallback = MoveTemp(OnFinished);
	CurrentSynthesisRequest = Request;

	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("text"), Request.Text);
	Json->SetStringField(TEXT("language"), Request.Language);
	Json->SetNumberField(TEXT("rate"), Request.Rate);
	Json->SetNumberField(TEXT("pitch"), Request.Pitch);
	Json->SetNumberField(TEXT("volume"), Request.Volume);
	if (!Request.VoiceId.IsEmpty())
	{
		Json->SetStringField(TEXT("voice"), Request.VoiceId);
	}
	if (!Settings->ModelName.IsEmpty())
	{
		Json->SetStringField(TEXT("model"), Settings->ModelName);
	}

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Json, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Settings->SynthesizeUrl);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Accept"), TEXT("audio/wav"));
	HttpRequest->SetTimeout(60.f);

	const FString ApiKey = Settings->ResolveApiKey();
	if (!ApiKey.IsEmpty())
	{
		HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	}

	HttpRequest->SetContentAsString(Body);
	HttpRequest->OnProcessRequestComplete().BindRaw(this, &FVoiceNTextRestProvider::HandleSynthesizeResponse);

	SynthesizeRequest = HttpRequest;
	HttpRequest->ProcessRequest();
	return true;
}

void FVoiceNTextRestProvider::CancelSynthesis()
{
	if (SynthesizeRequest.IsValid())
	{
		SynthesizeRequest->CancelRequest();
		SynthesizeRequest.Reset();
	}
}

void FVoiceNTextRestProvider::Shutdown()
{
	bShuttingDown = true;

	if (TranscribeRequest.IsValid())
	{
		TranscribeRequest->CancelRequest();
		TranscribeRequest.Reset();
	}
	if (SynthesizeRequest.IsValid())
	{
		SynthesizeRequest->CancelRequest();
		SynthesizeRequest.Reset();
	}

	ResetCallbacks();
	PendingPCM16.Reset();
}

void FVoiceNTextRestProvider::ResetCallbacks()
{
	TranscriptCallback = nullptr;
	AudioCallback = nullptr;
	ErrorCallback = nullptr;
	FinishedCallback = nullptr;
}

FVoiceNTextError FVoiceNTextRestProvider::MakeError(FHttpResponsePtr Response, bool bConnected, const TCHAR* Context) const
{
	FVoiceNTextError Error;

	if (!bConnected || !Response.IsValid())
	{
		Error.Code = EVoiceNTextErrorCode::Network;
		Error.Message = FString::Printf(TEXT("VoiceNText: %s request failed (no connection or DNS/TLS error)."), Context);
		return Error;
	}

	const int32 Status = Response->GetResponseCode();
	if (Status < 400)
	{
		return Error;
	}

	Error.HttpStatus = Status;
	Error.Code = (Status == 401 || Status == 403) ? EVoiceNTextErrorCode::Auth
		: (Status == 408 || Status == 504) ? EVoiceNTextErrorCode::Timeout
		: (Status == 429) ? EVoiceNTextErrorCode::Backend
		: (Status >= 500) ? EVoiceNTextErrorCode::Backend
		: EVoiceNTextErrorCode::InvalidRequest;

	FString Body = Response->GetContentAsString();
	Body.LeftInline(300);
	Error.Message = FString::Printf(TEXT("VoiceNText: %s failed with HTTP %d. %s"), Context, Status, *Body);
	return Error;
}

void FVoiceNTextRestProvider::EmitError(const FVoiceNTextError& Error)
{
	if (ErrorCallback)
	{
		ErrorCallback(Error);
	}
}

void FVoiceNTextRestProvider::EmitFinished()
{
	if (FinishedCallback)
	{
		FinishedCallback();
	}
}

void FVoiceNTextRestProvider::HandleTranscribeResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected)
{
	TranscribeRequest.Reset();
	if (bShuttingDown)
	{
		return;
	}

	const FVoiceNTextError Error = MakeError(Response, bConnected, TEXT("Transcription"));
	if (Error.IsError())
	{
		EmitError(Error);
		EmitFinished();
		return;
	}

	const FString Body = Response->GetContentAsString();

	FString Text;
	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
	{
		Json->TryGetStringField(TEXT("text"), Text);
	}

	if (Text.IsEmpty())
	{
		FVoiceNTextError ParseError;
		ParseError.Code = EVoiceNTextErrorCode::Backend;
		ParseError.HttpStatus = Response->GetResponseCode();
		ParseError.Message = FString::Printf(TEXT("VoiceNText: transcription response contained no 'text' field. Body: %s"), *Body.Left(200));
		EmitError(ParseError);
		EmitFinished();
		return;
	}

	FTranscriptSegment Segment;
	Segment.Text = Text;
	Segment.State = ETranscriptState::Final;
	Segment.StartSeconds = 0.0;
	Segment.EndSeconds = VoiceNText::Pcm::EstimateDurationMs(PendingPCM16, CurrentTranscriptionRequest.InputFormat) / 1000.0;

	if (TranscriptCallback)
	{
		TranscriptCallback(Segment);
	}

	if (!bShuttingDown)
	{
		EmitFinished();
	}
}

void FVoiceNTextRestProvider::HandleSynthesizeResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected)
{
	SynthesizeRequest.Reset();
	if (bShuttingDown)
	{
		return;
	}

	const FVoiceNTextError Error = MakeError(Response, bConnected, TEXT("Synthesis"));
	if (Error.IsError())
	{
		EmitError(Error);
		EmitFinished();
		return;
	}

	const UVoiceNTextSettings* Settings = UVoiceNTextSettings::Get();

	FAudioFormat OutFormat = Settings->SynthesizedAudioFormat;
	TArray<uint8> PCM16;
	VoiceNText::Pcm::ParseWavOrRaw(Response->GetContent(), Settings->SynthesizedAudioFormat, OutFormat, PCM16);

	if (PCM16.Num() == 0)
	{
		FVoiceNTextError EmptyError;
		EmptyError.Code = EVoiceNTextErrorCode::Backend;
		EmptyError.HttpStatus = Response->GetResponseCode();
		EmptyError.Message = TEXT("VoiceNText: synthesis response contained no audio payload.");
		EmitError(EmptyError);
		EmitFinished();
		return;
	}

	FAudioChunk Chunk;
	Chunk.PCM16 = MoveTemp(PCM16);
	Chunk.Format = OutFormat;

	if (AudioCallback)
	{
		AudioCallback(Chunk);
	}

	if (!bShuttingDown)
	{
		EmitFinished();
	}
}
