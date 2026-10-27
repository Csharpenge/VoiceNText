#include "VoiceNTextSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "VoiceNTextSettings.h"
#include "VoiceNTextSoundWave.h"

void UVoiceNTextSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ActiveProviderId = UVoiceNTextSettings::Get()->ProviderId;
	EnsureProvider();
}

void UVoiceNTextSubsystem::Deinitialize()
{
	if (Provider)
	{
		Provider->Shutdown();
		Provider.Reset();
	}

	StopPlayback();
	Super::Deinitialize();
}

void UVoiceNTextSubsystem::EnsureProvider()
{
	const FName Desired = ActiveProviderId.IsNone() ? UVoiceNTextSettings::Get()->ProviderId : ActiveProviderId;
	if (Provider.IsValid() && ActiveProviderId == Desired)
	{
		return;
	}

	if (Provider)
	{
		Provider->Shutdown();
		Provider.Reset();
	}

	ActiveProviderId = Desired;
	Provider = FVoiceNTextModule::Get().CreateProvider(Desired);

	if (!Provider)
	{
		TArray<FName> Known = FVoiceNTextModule::Get().GetProviderIds();
		FString KnownList;
		for (const FName& Id : Known)
		{
			KnownList += Id.ToString() + TEXT(" ");
		}
		UE_LOG(LogTemp, Warning, TEXT("VoiceNText: provider '%s' is not registered (known: %s)."), *Desired.ToString(), *KnownList.TrimEnd());
		ActiveProviderId = NAME_None;
	}
}

bool UVoiceNTextSubsystem::StartTranscription(const FSpeechToTextRequest& Request)
{
	if (bTranscribing)
	{
		Fail(EVoiceNTextTask::SpeechToText, EVoiceNTextErrorCode::Busy, TEXT("A transcription is already running."));
		return false;
	}

	EnsureProvider();
	if (!Provider.IsValid())
	{
		Fail(EVoiceNTextTask::SpeechToText, EVoiceNTextErrorCode::NotInitialized, TEXT("No VoiceNText provider is registered."));
		return false;
	}

	FString Reason;
	if (!Provider->IsAvailable(Reason))
	{
		Fail(EVoiceNTextTask::SpeechToText, EVoiceNTextErrorCode::Unsupported, Reason);
		return false;
	}

	const bool bStarted = Provider->BeginTranscription(
		Request,
		[this](const FTranscriptSegment& Segment) { HandleTranscript(Segment); },
		[this](const FVoiceNTextError& Error) { HandleProviderError(EVoiceNTextTask::SpeechToText, Error); },
		[this]() { HandleTranscriptionFinished(); });

	if (!bStarted)
	{
		Fail(EVoiceNTextTask::SpeechToText, EVoiceNTextErrorCode::Backend, TEXT("The provider rejected the transcription request (check the configured endpoint)."));
		return false;
	}

	bTranscribing = true;
	return true;
}

void UVoiceNTextSubsystem::PushAudio(const TArray<uint8>& PCM16)
{
	if (bTranscribing && Provider.IsValid() && PCM16.Num() > 0)
	{
		Provider->PushAudio(PCM16);
	}
}

void UVoiceNTextSubsystem::EndTranscription()
{
	if (Provider.IsValid() && bTranscribing)
	{
		Provider->EndTranscription();
	}
}

void UVoiceNTextSubsystem::CancelTranscription()
{
	if (Provider.IsValid())
	{
		Provider->CancelTranscription();
	}
	bTranscribing = false;
}

bool UVoiceNTextSubsystem::StartSynthesis(const FTextToSpeechRequest& Request)
{
	if (bSynthesizing)
	{
		Fail(EVoiceNTextTask::TextToSpeech, EVoiceNTextErrorCode::Busy, TEXT("A synthesis is already running."));
		return false;
	}

	if (Request.Text.IsEmpty())
	{
		Fail(EVoiceNTextTask::TextToSpeech, EVoiceNTextErrorCode::InvalidRequest, TEXT("TextToSpeech request has an empty text."));
		return false;
	}

	EnsureProvider();
	if (!Provider.IsValid())
	{
		Fail(EVoiceNTextTask::TextToSpeech, EVoiceNTextErrorCode::NotInitialized, TEXT("No VoiceNText provider is registered."));
		return false;
	}

	FString Reason;
	if (!Provider->IsAvailable(Reason))
	{
		Fail(EVoiceNTextTask::TextToSpeech, EVoiceNTextErrorCode::Unsupported, Reason);
		return false;
	}

	if (UVoiceNTextSettings::Get()->bAutoPlaySynthesis)
	{
		StopPlayback();
	}

	const bool bStarted = Provider->BeginSynthesis(
		Request,
		[this](const FAudioChunk& Chunk) { HandleAudioChunk(Chunk); },
		[this](const FVoiceNTextError& Error) { HandleProviderError(EVoiceNTextTask::TextToSpeech, Error); },
		[this]() { HandleSynthesisFinished(); });

	if (!bStarted)
	{
		Fail(EVoiceNTextTask::TextToSpeech, EVoiceNTextErrorCode::Backend, TEXT("The provider rejected the synthesis request (check the configured endpoint)."));
		return false;
	}

	bSynthesizing = true;
	return true;
}

void UVoiceNTextSubsystem::CancelSynthesis()
{
	if (Provider.IsValid())
	{
		Provider->CancelSynthesis();
	}
	bSynthesizing = false;
	StopPlayback();
}

void UVoiceNTextSubsystem::CancelAll()
{
	CancelTranscription();
	CancelSynthesis();
}

void UVoiceNTextSubsystem::SetActiveProvider(FName InProviderId)
{
	if (ActiveProviderId == InProviderId)
	{
		return;
	}

	CancelAll();
	ActiveProviderId = InProviderId;
	EnsureProvider();
}

void UVoiceNTextSubsystem::Fail(EVoiceNTextTask Task, EVoiceNTextErrorCode Code, const FString& Message, int32 HttpStatus)
{
	FVoiceNTextError Error;
	Error.Code = Code;
	Error.Message = Message;
	Error.HttpStatus = HttpStatus;

	UE_LOG(LogTemp, Warning, TEXT("VoiceNText[%s]: %s"), Task == EVoiceNTextTask::SpeechToText ? TEXT("STT") : TEXT("TTS"), *Message);
	OnFailed.Broadcast(Task, Error);
}

void UVoiceNTextSubsystem::HandleTranscript(const FTranscriptSegment& Segment)
{
	OnTranscript.Broadcast(Segment);
}

void UVoiceNTextSubsystem::HandleAudioChunk(const FAudioChunk& Chunk)
{
	OnAudioChunk.Broadcast(Chunk);

	const UVoiceNTextSettings* Settings = UVoiceNTextSettings::Get();
	if (!Settings->bAutoPlaySynthesis || Chunk.PCM16.Num() == 0)
	{
		return;
	}

	UWorld* World = GetGameWorld();
	if (!World)
	{
		return;
	}

	if (!ActiveWave || !ActiveAudioComponent)
	{
		ActiveWave = NewObject<UVoiceNTextSoundWave>(this);
		ActiveWave->InitializeForFormat(Chunk.Format);
		ActiveAudioComponent = UGameplayStatics::SpawnSound2D(World, ActiveWave);
	}

	ActiveWave->EnqueuePCM16(Chunk.PCM16);
}

void UVoiceNTextSubsystem::HandleProviderError(EVoiceNTextTask Task, const FVoiceNTextError& Error)
{
	if (Task == EVoiceNTextTask::SpeechToText)
	{
		bTranscribing = false;
	}
	else
	{
		bSynthesizing = false;
	}

	UE_LOG(LogTemp, Warning, TEXT("VoiceNText[%s] failed: %s"), Task == EVoiceNTextTask::SpeechToText ? TEXT("STT") : TEXT("TTS"), *Error.Message);
	OnFailed.Broadcast(Task, Error);
}

void UVoiceNTextSubsystem::HandleTranscriptionFinished()
{
	bTranscribing = false;
	OnFinished.Broadcast(EVoiceNTextTask::SpeechToText, FVoiceNTextError());
}

void UVoiceNTextSubsystem::HandleSynthesisFinished()
{
	bSynthesizing = false;

	if (ActiveWave)
	{
		ActiveWave->MarkFinished();

		if (UWorld* World = GetGameWorld())
		{
			World->GetTimerManager().SetTimer(
				PlaybackTimerHandle,
				FTimerDelegate::CreateUObject(this, &UVoiceNTextSubsystem::CheckPlaybackFinished),
				0.1f,
				true);
		}
	}

	OnFinished.Broadcast(EVoiceNTextTask::TextToSpeech, FVoiceNTextError());
}

void UVoiceNTextSubsystem::CheckPlaybackFinished()
{
	if (!ActiveWave || ActiveWave->IsPlaybackFinished())
	{
		StopPlayback();
	}
}

void UVoiceNTextSubsystem::StopPlayback()
{
	if (UWorld* World = GetGameWorld())
	{
		World->GetTimerManager().ClearTimer(PlaybackTimerHandle);
	}

	if (ActiveAudioComponent)
	{
		ActiveAudioComponent->Stop();
		ActiveAudioComponent = nullptr;
	}

	if (ActiveWave)
	{
		ActiveWave->ResetAudio();
		ActiveWave = nullptr;
	}
}

UWorld* UVoiceNTextSubsystem::GetGameWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}
