#include "VoiceNTextCaptureComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "VoiceNTextPcm.h"
#include "VoiceNTextSubsystem.h"

UVoiceNTextCaptureComponent::UVoiceNTextCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	OutputFormat.SampleRate = 16000;
	OutputFormat.NumChannels = 1;
	OutputFormat.BitsPerSample = 16;
}

bool UVoiceNTextCaptureComponent::StartCapture()
{
	if (bCapturing)
	{
		return true;
	}

	Capture = MakeUnique<Audio::FAudioCapture>();

	Audio::FAudioCaptureDeviceParams Params;
	Params.PCMAudioEncoding = Audio::EPCMAudioEncoding::FLOATING_POINT_32;

	const bool bOpened = Capture->OpenAudioCaptureStream(
		Params,
		[this](const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverflow)
		{
			OnDeviceAudio(InAudio, NumFrames, NumChannels, SampleRate, StreamTime, bOverflow);
		},
		1024);

	if (!bOpened || !Capture->StartStream())
	{
		Capture->CloseStream();
		Capture.Reset();
		UE_LOG(LogTemp, Warning, TEXT("VoiceNText: failed to open / start the microphone stream."));
		return false;
	}

	bCapturing = true;
	SetComponentTickEnabled(true);
	return true;
}

void UVoiceNTextCaptureComponent::StopCapture()
{
	if (Capture.IsValid())
	{
		Capture->StopStream();
		Capture->CloseStream();
		Capture.Reset();
	}

	bCapturing = false;
	SetComponentTickEnabled(false);
	DrainPendingChunks();
}

void UVoiceNTextCaptureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCapture();
	Super::EndPlay(EndPlayReason);
}

void UVoiceNTextCaptureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bCapturing)
	{
		DrainPendingChunks();
	}
}

void UVoiceNTextCaptureComponent::OnDeviceAudio(const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverflow)
{
	if (!InAudio || NumFrames <= 0)
	{
		return;
	}

	FAudioChunk Chunk;
	Chunk.Format.SampleRate = (SampleRate > 0) ? SampleRate : OutputFormat.SampleRate;
	Chunk.Format.NumChannels = FMath::Max(1, NumChannels);
	Chunk.Format.BitsPerSample = 16;

	VoiceNText::Pcm::FloatToPCM16(static_cast<const float*>(InAudio), NumFrames, Chunk.Format.NumChannels, Chunk.PCM16);

	// Producer side runs on the audio capture thread; SPSC queue keeps this lock-free.
	PendingChunks.Enqueue(MoveTemp(Chunk));
}

void UVoiceNTextCaptureComponent::DrainPendingChunks()
{
	FAudioChunk Chunk;
	while (PendingChunks.Dequeue(Chunk))
	{
		if (Chunk.Format.SampleRate != OutputFormat.SampleRate || Chunk.Format.NumChannels != OutputFormat.NumChannels)
		{
			TArray<uint8> Resampled;
			VoiceNText::Pcm::ResamplePCM16(Chunk.PCM16, Chunk.Format.SampleRate, Chunk.Format.NumChannels, OutputFormat.SampleRate, Resampled);
			Chunk.PCM16 = MoveTemp(Resampled);
			Chunk.Format.SampleRate = OutputFormat.SampleRate;
		}

		OnAudioChunk.Broadcast(Chunk);

		if (bForwardToSubsystem)
		{
			const UWorld* World = GetWorld();
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			if (UVoiceNTextSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UVoiceNTextSubsystem>() : nullptr)
			{
				Subsystem->PushAudio(Chunk.PCM16);
			}
		}
	}
}
