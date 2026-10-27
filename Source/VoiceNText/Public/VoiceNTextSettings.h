#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoiceNTextTypes.h"
#include "VoiceNTextSettings.generated.h"

/**
 * Project settings for the VoiceNText plugin (Project Settings > Plugins > Voice N Text).
 * Editable from the editor toolbar entry as well.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Voice N Text"))
class VOICENTEXT_API UVoiceNTextSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoiceNTextSettings();

	static const UVoiceNTextSettings* Get() { return GetDefault<UVoiceNTextSettings>(); }
	static UVoiceNTextSettings* GetMutable() { return GetMutableDefault<UVoiceNTextSettings>(); }

	/** Registered provider id: "Null" or "Rest". */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	FName ProviderId = TEXT("Null");

	/** REST endpoint that accepts a WAV upload and answers with {"text": "..."}. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider|Rest")
	FString TranscribeUrl;

	/** REST endpoint that accepts {"text": "..."} and answers with WAV/RAW PCM16 audio. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider|Rest")
	FString SynthesizeUrl;

	/** Environment variable read at runtime for the bearer token. Takes priority over ApiKey. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider|Rest")
	FString ApiKeyEnvVar = TEXT("VOICENTEXT_API_KEY");

	/** Fallback token stored in config. Do not ship builds with this filled in. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider|Rest", meta = (DisplayName = "API Key (insecure fallback)"))
	FString ApiKey;

	/** Optional model / voice profile forwarded to the endpoint. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider|Rest")
	FString ModelName;

	/** Format assumed when a synthesis response is raw PCM instead of a WAV container. */
	UPROPERTY(Config, EditAnywhere, Category = "Playback")
	FAudioFormat SynthesizedAudioFormat;

	/** Automatically play synthesized audio through SpawnSound2D. */
	UPROPERTY(Config, EditAnywhere, Category = "Playback")
	bool bAutoPlaySynthesis = true;

	/** Enqueue audio chunks while they arrive instead of waiting for the whole response. */
	UPROPERTY(Config, EditAnywhere, Category = "Playback")
	bool bStreamingPlayback = true;

	/** Defaults used when callers pass an empty request. */
	UPROPERTY(Config, EditAnywhere, Category = "Defaults")
	FSpeechToTextRequest DefaultTranscription;

	UPROPERTY(Config, EditAnywhere, Category = "Defaults")
	FTextToSpeechRequest DefaultSynthesis;

	/** Bearer token resolved from the environment first, then from ApiKey. */
	FString ResolveApiKey() const;

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~ End UDeveloperSettings
};
