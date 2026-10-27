#include "VoiceNTextPcm.h"

namespace
{
	void AppendUInt32LE(TArray<uint8>& Out, uint32 Value)
	{
		Out.Add(static_cast<uint8>(Value & 0xFF));
		Out.Add(static_cast<uint8>((Value >> 8) & 0xFF));
		Out.Add(static_cast<uint8>((Value >> 16) & 0xFF));
		Out.Add(static_cast<uint8>((Value >> 24) & 0xFF));
	}

	void AppendUInt16LE(TArray<uint8>& Out, uint16 Value)
	{
		Out.Add(static_cast<uint8>(Value & 0xFF));
		Out.Add(static_cast<uint8>((Value >> 8) & 0xFF));
	}

	uint32 ReadUInt32LE(const uint8* Data)
	{
		return static_cast<uint32>(Data[0]) | (static_cast<uint32>(Data[1]) << 8) |
			(static_cast<uint32>(Data[2]) << 16) | (static_cast<uint32>(Data[3]) << 24);
	}

	uint16 ReadUInt16LE(const uint8* Data)
	{
		return static_cast<uint16>(Data[0]) | (static_cast<uint16>(Data[1]) << 8);
	}

	int16 FloatToSample(float Value)
	{
		const float Clamped = FMath::Clamp(Value, -1.f, 1.f);
		return static_cast<int16>(FMath::RoundToInt(Clamped * 32767.f));
	}
}

void VoiceNText::Pcm::FloatToPCM16(const float* InFrames, int32 NumFrames, int32 NumChannels, TArray<uint8>& OutPCM16)
{
	if (!InFrames || NumFrames <= 0 || NumChannels <= 0)
	{
		OutPCM16.Reset();
		return;
	}

	const int32 NumSamples = NumFrames * NumChannels;
	OutPCM16.SetNumUninitialized(NumSamples * sizeof(int16));

	int16* Out = reinterpret_cast<int16*>(OutPCM16.GetData());
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		Out[Index] = FloatToSample(InFrames[Index]);
	}
}

void VoiceNText::Pcm::ResamplePCM16(const TArray<uint8>& InPCM16, int32 InSampleRate, int32 NumChannels, int32 OutSampleRate, TArray<uint8>& OutPCM16)
{
	if (InSampleRate <= 0 || OutSampleRate <= 0 || NumChannels <= 0)
	{
		OutPCM16 = InPCM16;
		return;
	}

	const int32 InSamplesPerChannel = InPCM16.Num() / (sizeof(int16) * NumChannels);
	if (InSamplesPerChannel <= 0)
	{
		OutPCM16.Reset();
		return;
	}

	if (InSampleRate == OutSampleRate)
	{
		OutPCM16 = InPCM16;
		return;
	}

	const int32 OutSamplesPerChannel = FMath::Max(1, FMath::RoundToInt(InSamplesPerChannel * (static_cast<double>(OutSampleRate) / InSampleRate)));
	OutPCM16.SetNumUninitialized(OutSamplesPerChannel * NumChannels * sizeof(int16));

	const int16* In = reinterpret_cast<const int16*>(InPCM16.GetData());
	int16* Out = reinterpret_cast<int16*>(OutPCM16.GetData());

	const double Step = static_cast<double>(InSamplesPerChannel) / OutSamplesPerChannel;
	for (int32 Frame = 0; Frame < OutSamplesPerChannel; ++Frame)
	{
		const double SourcePos = Frame * Step;
		const int32 Index0 = FMath::Clamp(static_cast<int32>(SourcePos), 0, InSamplesPerChannel - 1);
		const int32 Index1 = FMath::Min(Index0 + 1, InSamplesPerChannel - 1);
		const float Alpha = static_cast<float>(SourcePos - Index0);

		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			const float SampleA = In[Index0 * NumChannels + Channel];
			const float SampleB = In[Index1 * NumChannels + Channel];
			Out[Frame * NumChannels + Channel] = static_cast<int16>(FMath::RoundToInt(FMath::Lerp(SampleA, SampleB, Alpha)));
		}
	}
}

void VoiceNText::Pcm::WrapPcm16AsWav(const TArray<uint8>& PCM16, const FAudioFormat& Format, TArray<uint8>& OutWav)
{
	const int32 NumChannels = FMath::Max(1, Format.NumChannels);
	const int32 SampleRate = FMath::Max(8000, Format.SampleRate);
	const int32 BitsPerSample = 16;
	const int32 BlockAlign = NumChannels * BitsPerSample / 8;
	const int32 ByteRate = SampleRate * BlockAlign;
	const int32 DataBytes = PCM16.Num();

	OutWav.Reset(44 + DataBytes);
	OutWav.Append(reinterpret_cast<const uint8*>("RIFF"), 4);
	AppendUInt32LE(OutWav, 36 + DataBytes);
	OutWav.Append(reinterpret_cast<const uint8*>("WAVE"), 4);
	OutWav.Append(reinterpret_cast<const uint8*>("fmt "), 4);
	AppendUInt32LE(OutWav, 16);
	AppendUInt16LE(OutWav, 1); // PCM
	AppendUInt16LE(OutWav, static_cast<uint16>(NumChannels));
	AppendUInt32LE(OutWav, static_cast<uint32>(SampleRate));
	AppendUInt32LE(OutWav, static_cast<uint32>(ByteRate));
	AppendUInt16LE(OutWav, static_cast<uint16>(BlockAlign));
	AppendUInt16LE(OutWav, static_cast<uint16>(BitsPerSample));
	OutWav.Append(reinterpret_cast<const uint8*>("data"), 4);
	AppendUInt32LE(OutWav, static_cast<uint32>(DataBytes));
	OutWav.Append(PCM16);
}

void VoiceNText::Pcm::ParseWavOrRaw(const TArray<uint8>& InBytes, const FAudioFormat& FallbackFormat, FAudioFormat& OutFormat, TArray<uint8>& OutPCM16)
{
	OutFormat = FallbackFormat;
	OutPCM16.Reset();

	const bool bLooksLikeWav = InBytes.Num() > 44 &&
		FMemory::Memcmp(InBytes.GetData(), "RIFF", 4) == 0 &&
		FMemory::Memcmp(InBytes.GetData() + 8, "WAVE", 4) == 0;

	if (!bLooksLikeWav)
	{
		OutPCM16 = InBytes;
		return;
	}

	const uint8* Data = InBytes.GetData();
	int32 Offset = 12;

	while (Offset + 8 <= InBytes.Num())
	{
		const uint32 ChunkSize = ReadUInt32LE(Data + Offset + 4);
		const bool bIsFmt = FMemory::Memcmp(Data + Offset, "fmt ", 4) == 0;
		const bool bIsData = FMemory::Memcmp(Data + Offset, "data", 4) == 0;

		if (bIsFmt && Offset + 8 + 16 <= InBytes.Num())
		{
			const uint8* Fmt = Data + Offset + 8;
			OutFormat.NumChannels = ReadUInt16LE(Fmt + 2);
			OutFormat.SampleRate = static_cast<int32>(ReadUInt32LE(Fmt + 4));
			OutFormat.BitsPerSample = ReadUInt16LE(Fmt + 14);
		}
		else if (bIsData)
		{
			const int32 DataStart = Offset + 8;
			const int32 ByteCount = FMath::Min(static_cast<int32>(ChunkSize), InBytes.Num() - DataStart);
			if (ByteCount > 0)
			{
				OutPCM16.Append(Data + DataStart, ByteCount);
			}
			break;
		}

		Offset += 8 + static_cast<int32>(ChunkSize) + (ChunkSize % 2);
	}

	if (OutPCM16.Num() == 0)
	{
		OutFormat = FallbackFormat;
		OutPCM16 = InBytes;
	}
}

int32 VoiceNText::Pcm::EstimateDurationMs(const TArray<uint8>& PCM16, const FAudioFormat& Format)
{
	const int32 BlockAlign = FMath::Max(1, Format.NumChannels * FMath::Max(8, Format.BitsPerSample) / 8);
	const int32 NumFrames = PCM16.Num() / BlockAlign;
	if (Format.SampleRate <= 0)
	{
		return 0;
	}
	return FMath::RoundToInt(1000.0 * NumFrames / Format.SampleRate);
}
