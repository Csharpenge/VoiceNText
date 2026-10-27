# VoiceNText

**[English](README.md) | [简体中文](README.zh-CN.md)**

基于 Unreal Engine **5.8** 的插件，通过云端后端实现流式**语音识别（ASR）**与**语音合成（TTS）**，内置麦克风采集与程序化音频播放。

状态：**实验性 / Beta** —— 框架已完成并可编译；内置的 `Rest` 后端面向通用 HTTP 服务，需要接入真实端点才能实际工作。

## 功能

- 基于 Provider 的架构：`IVoiceNTextProvider` 实现在模块启动时注册，按名称选择。
- 单一蓝图门面：`UVoiceNTextSubsystem`（GameInstance 子系统），提供动态多播事件。
- 麦克风采集：`UVoiceNTextCaptureComponent`（设备 float32 → PCM16，默认线性重采样到 16 kHz 单声道）。
- 流式播放：`UVoiceNTextSoundWave`（`USoundWaveProcedural`）在音频分块到达时即时入队播放。
- 编辑器设置：**Play** 旁的工具栏按钮打开绑定到 `UVoiceNTextSettings` 的详情面板，修改会保存到 `Config/DefaultGame.ini`。
- 内置后端：`Null`（占位）与 `Rest`（通用 HTTP 请求/响应）。

## 环境要求

- Unreal Engine 5.8（开发时使用 Windows 编辑器目标）。
- 麦克风采集路径目前需要 Windows。

## 安装

将 `VoiceNText` 目录复制到工程的 `Plugins/` 下（或直接 clone 到该位置），并在 `.uproject` 中启用插件：

```json
"Plugins": [
    { "Name": "VoiceNText", "Enabled": true }
]
```

随后重新生成工程文件并编译编辑器目标。

## 配置

工具栏 **Voice N Text** 按钮（Level Editor → Play 工具栏）或 **Project Settings → Plugins → Voice N Text**。

| 设置项 | 含义 |
| --- | --- |
| `ProviderId` | `Null`（默认）或 `Rest` |
| `TranscribeUrl` | 接收 WAV 上传的 POST 端点 |
| `SynthesizeUrl` | 接收 JSON 请求体的 POST 端点 |
| `ApiKeyEnvVar` | 存放 Bearer Token 的环境变量名（默认 `VOICENTEXT_API_KEY`） |
| `ApiKey` | 不安全的备用 Token —— 正式打包请留空 |
| `bAutoPlaySynthesis` | 通过 `SpawnSound2D` 自动播放合成音频 |
| `SynthesizedAudioFormat` | 当 TTS 返回裸 PCM16 而非 WAV 时假定的格式 |

### REST 约定

- **ASR** —— 请求：`POST TranscribeUrl`，`Content-Type: audio/wav`，可选 `Authorization: Bearer <token>`；响应：包含 `text` 字段的 JSON。
- **TTS** —— 请求：`POST SynthesizeUrl`，JSON `{ "text", "voice", "language", "rate", "pitch", "volume", "model"? }`；响应：WAV（`RIFF`）或裸 PCM16（使用 `SynthesizedAudioFormat`）。

## 用法（C++）

```cpp
UVoiceNTextSubsystem* Voice = GetGameInstance()->GetSubsystem<UVoiceNTextSubsystem>();

FSpeechToTextRequest Listen;
Voice->StartTranscription(Listen);   // 采集组件会自动推送 PCM
Voice->EndTranscription();           // 最终转写结果通过 OnTranscript 返回

FTextToSpeechRequest Speak;
Speak.Text = TEXT("Hello from VoiceNText");
Voice->StartSynthesis(Speak);
```

采集组件（蓝图或 C++）：

```cpp
UVoiceNTextCaptureComponent* Mic = NewObject<UVoiceNTextCaptureComponent>(this);
Mic->RegisterComponent();
Mic->StartCapture();   // 默认把 PCM16 分块转发给子系统
```

## 用法（蓝图）

- 在 `VoiceNTextSubsystem` 节点上调用 **Start Transcription**，绑定 **On Transcript**，然后调用 **End Transcription**。
- 用 `Text To Speech Request` 调用 **Start Synthesis**；若要自行处理 PCM，绑定 **On Audio Chunk** 并关闭 `bAutoPlaySynthesis`。
- **On Failed** 会携带 `FVoiceNTextError`（`Code` / `Message` / `HttpStatus`）。

## 架构

| 文件 | 职责 |
| --- | --- |
| `Public/VoiceNText.h` | `IVoiceNTextProvider` 接口、模块与 Provider 注册表 |
| `Public/VoiceNTextTypes.h` | 枚举、请求/结果结构体、动态委托 |
| `Public/VoiceNTextSubsystem.h` | 门面、播放编排、错误转发 |
| `Public/VoiceNTextCaptureComponent.h` | 麦克风采集与重采样 |
| `Public/VoiceNTextSoundWave.h` | 流式 PCM16 播放 |
| `Private/Providers/VoiceNTextRestProvider.*` | 通用 REST 后端 |
| `Source/VoiceNTextEditor` | 工具栏入口与设置标签页 |

## 新增后端

实现 `IVoiceNTextProvider`，然后在你自己的模块 `StartupModule` 中注册：

```cpp
FVoiceNTextModule::Get().RegisterProvider(TEXT("MyVendor"),
    []() { return MakeUnique<FMyVendorProvider>(); });
```

运行时通过 `UVoiceNTextSubsystem::SetActiveProvider(TEXT("MyVendor"))` 切换。

## 已知限制

- 内置 `Rest` 后端是请求/响应式：每次调用只产生一段转写文本与一段音频。若需要真正的增量流式（部分转写、低延迟播放），请新增 WebSocket/SSE 后端。
- 当前实现的麦克风采集仅支持 Windows。
- 插件标记为 Beta，API 可能变化。
