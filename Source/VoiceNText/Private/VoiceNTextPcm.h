#pragma once

#include "CoreMinimal.h"
#include "VoiceNTextTypes.h"

namespace VoiceNText
{
	namespace Pcm
	{
		/** Interleaved float [-1,1] frames -> little-endian PCM16. */
		void FloatToPCM16(const float* InFrames, int32 NumFrames, int32 NumChannels, TArray<uint8>& OutPCM16);

		/** Linear resampling of interleaved PCM16. */
		void ResamplePCM16(const TArray<uint8>& InPCM16, int32 InSampleRate, int32 NumChannels, int32 OutSampleRate, TArray<uint8>& OutPCM16);

		/** Canonical 44-byte RIFF/WAVE header followed by the PCM payload. */
		void WrapPcm16AsWav(const TArray<uint8>& PCM16, const FAudioFormat& Format, TArray<uint8>& OutWav);

		/** Accepts a RIFF/WAVE file or raw PCM16; returns the usable format and payload. */
		void ParseWavOrRaw(const TArray<uint8>& InBytes, const FAudioFormat& FallbackFormat, FAudioFormat& OutFormat, TArray<uint8>& OutPCM16);

		int32 EstimateDurationMs(const TArray<uint8>& PCM16, const FAudioFormat& Format);
	}
}
