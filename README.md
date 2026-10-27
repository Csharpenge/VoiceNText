# VoiceNText

**[English](README.md) | [简体中文](README.zh-CN.md)**

Unreal Engine **5.8** plugin for streaming **speech-to-text (ASR)** and **text-to-speech (TTS)** through cloud backends, with microphone capture and procedural audio playback.

Status: **experimental / beta** — the framework is complete and compiles; the bundled `Rest` backend targets generic HTTP services and needs a real endpoint to be useful.

## Features

- Provider-based architecture: `IVoiceNTextProvider` implementations are registered at module startup and selected by name.
- Single Blueprint-facing facade: `UVoiceNTextSubsystem` (game instance subsystem) with dynamic multicast events.
- Microphone capture: `UVoiceNTextCaptureComponent` (device float32 → PCM16, linear resampling to 16 kHz mono by default).
- Streaming playback: `UVoiceNTextSoundWave` (`USoundWaveProcedural`) accepts PCM16 chunks while they arrive.
- Editor settings: toolbar button next to **Play** opens a details panel bound to `UVoiceNTextSettings`; edits are saved to `Config/DefaultGame.ini`.
- Bundled backends: `Null` (placeholder) and `Rest` (generic HTTP request/response).

## Requirements

- Unreal Engine 5.8 (Windows editor target used during development).
- Windows for the microphone capture path.

## Installation

Copy the `VoiceNText` folder into your project's `Plugins/` directory (or clone it there) and add the plugin to your `.uproject`:

```json
"Plugins": [
    { "Name": "VoiceNText", "Enabled": true }
]
```

Regenerate project files and build the editor target.

## Configuration

Toolbar **Voice N Text** button (Level Editor → Play toolbar) or **Project Settings → Plugins → Voice N Text**.

| Setting | Meaning |
| --- | --- |
| `ProviderId` | `Null` (default) or `Rest` |
| `TranscribeUrl` | POST endpoint receiving a WAV upload |
| `SynthesizeUrl` | POST endpoint receiving a JSON body |
| `ApiKeyEnvVar` | Environment variable holding the bearer token (default `VOICENTEXT_API_KEY`) |
| `ApiKey` | Insecure fallback token — leave empty for shipped builds |
| `bAutoPlaySynthesis` | Play synthesized audio automatically via `SpawnSound2D` |
| `SynthesizedAudioFormat` | Format assumed when the TTS response is raw PCM16 instead of WAV |

### REST contract

- **ASR** — request: `POST TranscribeUrl`, `Content-Type: audio/wav`, optional `Authorization: Bearer <token>`; response: JSON with a `text` field.
- **TTS** — request: `POST SynthesizeUrl`, JSON `{ "text", "voice", "language", "rate", "pitch", "volume", "model"? }`; response: WAV (`RIFF`) or raw PCM16 (uses `SynthesizedAudioFormat`).

## Usage (C++)

```cpp
UVoiceNTextSubsystem* Voice = GetGameInstance()->GetSubsystem<UVoiceNTextSubsystem>();

FSpeechToTextRequest Listen;
Voice->StartTranscription(Listen);   // capture component feeds PCM automatically
Voice->EndTranscription();           // final transcript arrives on OnTranscript

FTextToSpeechRequest Speak;
Speak.Text = TEXT("Hello from VoiceNText");
Voice->StartSynthesis(Speak);
```

Capture component (BP or C++):

```cpp
UVoiceNTextCaptureComponent* Mic = NewObject<UVoiceNTextCaptureComponent>(this);
Mic->RegisterComponent();
Mic->StartCapture();   // forwards PCM16 chunks to the subsystem by default
```

## Usage (Blueprint)

- Call **Start Transcription** on the `VoiceNTextSubsystem` node, bind **On Transcript**, then call **End Transcription**.
- Call **Start Synthesis** with a `Text To Speech Request`, bind **On Audio Chunk** if you want to handle PCM yourself (disable `bAutoPlaySynthesis`).
- **On Failed** carries a `FVoiceNTextError` with `Code` / `Message` / `HttpStatus`.

## Architecture

| File | Responsibility |
| --- | --- |
| `Public/VoiceNText.h` | `IVoiceNTextProvider` interface, module + provider registry |
| `Public/VoiceNTextTypes.h` | Enums, request/result structs, dynamic delegates |
| `Public/VoiceNTextSubsystem.h` | Facade, playback orchestration, error routing |
| `Public/VoiceNTextCaptureComponent.h` | Microphone capture and resampling |
| `Public/VoiceNTextSoundWave.h` | Streaming PCM16 playback |
| `Private/Providers/VoiceNTextRestProvider.*` | Generic REST backend |
| `Source/VoiceNTextEditor` | Toolbar entry and settings tab |

## Adding a backend

Implement `IVoiceNTextProvider`, then register it in your own module's `StartupModule`:

```cpp
FVoiceNTextModule::Get().RegisterProvider(TEXT("MyVendor"),
    []() { return MakeUnique<FMyVendorProvider>(); });
```

Switch at runtime with `UVoiceNTextSubsystem::SetActiveProvider(TEXT("MyVendor"))`.

## Known limitations

- The bundled `Rest` backend is request/response: one transcript segment and one audio chunk per call. For true incremental streaming (partial transcripts, low-latency playback) add a WebSocket/SSE provider.
- Microphone capture is Windows-only in the current implementation.
- The plugin is marked beta; APIs may change.
