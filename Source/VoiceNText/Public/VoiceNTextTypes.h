#pragma once

#include "CoreMinimal.h"
#include "VoiceNTextTypes.generated.h"

/** Error codes shared by every VoiceNText backend. */
UENUM(BlueprintType)
enum class EVoiceNTextErrorCode : uint8
{
	None			UMETA(DisplayName = "None"),
	NotInitialized	UMETA(DisplayName = "Not Initialized"),
	Unsupported		UMETA(DisplayName = "Unsupported"),
	Busy			UMETA(DisplayName = "Busy"),
	InvalidRequest	UMETA(DisplayName = "Invalid Request"),
	Network			UMETA(DisplayName = "Network"),
	Auth			UMETA(DisplayName = "Auth"),
	Timeout			UMETA(DisplayName = "Timeout"),
	Backend			UMETA(DisplayName = "Backend"),
	Cancelled		UMETA(DisplayName = "Cancelled")
};

/** Which task a callback belongs to. */
UENUM(BlueprintType)
enum class EVoiceNTextTask : uint8
{
	SpeechToText	UMETA(DisplayName = "Speech To Text"),
	TextToSpeech	UMETA(DisplayName = "Text To Speech")
};

UENUM(BlueprintType)
enum class ETranscriptState : uint8
{
	Partial	UMETA(DisplayName = "Partial"),
	Final	UMETA(DisplayName = "Final")
};

USTRUCT(BlueprintType)
struct VOICENTEXT_API FVoiceNTextError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VoiceNText")
	EVoiceNTextErrorCode Code = EVoiceNTextErrorCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "VoiceNText")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "VoiceNText")
	int32 HttpStatus = 0;

	bool IsError() const { return Code != EVoiceNTextErrorCode::None; }
};

USTRUCT(BlueprintType)
struct VOICENTEXT_API FAudioFormat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText")
	int32 SampleRate = 16000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText")
	int32 NumChannels = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText")
	int32 BitsPerSample = 16;
};

/** A piece of transcribed text; streaming backends emit Partial updates before Final. */
USTRUCT(BlueprintType)
struct VOICENTEXT_API FTranscriptSegment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VoiceNText")
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category = "VoiceNText")
	ETranscriptState State = ETranscriptState::Final;

	UPROPERTY(BlueprintReadOnly, Category = "VoiceNText")
	float Confidence = -1.f;

	UPROPERTY(BlueprintReadOnly, Category = "VoiceNText")
	double StartSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "VoiceNText")
	double EndSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct VOICENTEXT_API FSpeechToTextRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText")
	FString Language = TEXT("zh-CN");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText")
	bool bInterimResults = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText")
	float MaxDurationSeconds = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText")
	FAudioFormat InputFormat;
};

USTRUCT(BlueprintType)
struct VOICENTEXT_API FTextToSpeechRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText")
	FString Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText")
	FString VoiceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText")
	FString Language = TEXT("zh-CN");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float Rate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float Pitch = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VoiceNText", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Volume = 1.f;
};

/** One block of synthesized PCM16 audio. */
USTRUCT(BlueprintType)
struct VOICENTEXT_API FAudioChunk
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VoiceNText")
	TArray<uint8> PCM16;

	UPROPERTY(BlueprintReadOnly, Category = "VoiceNText")
	FAudioFormat Format;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceNTextTranscript, const FTranscriptSegment&, Segment);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceNTextAudioChunk, const FAudioChunk&, Chunk);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVoiceNTextFinished, EVoiceNTextTask, Task, const FVoiceNTextError&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVoiceNTextFailed, EVoiceNTextTask, Task, const FVoiceNTextError&, Error);
