#include "pch.h"
#include "MediaRecorder.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

namespace Capture_Gemini
{
    MediaRecorder::MediaRecorder() = default;

    MediaRecorder::~MediaRecorder()
    {
        StopAudioCapture();
    }

    IAsyncOperation<bool> MediaRecorder::StartAsync(
        Direct3DHelper& d3dHelper,
        winrt::Windows::Storage::Streams::IRandomAccessStream outputStream,
        SizeInt32 captureSize,
        RecordingOptions options)
    {
        if (m_isRecording) co_return false;

        m_d3dHelper = &d3dHelper;
        m_outputStream = outputStream;

        uint32_t divisor = (options.scaleDivisor == 2 || options.scaleDivisor == 4) ? options.scaleDivisor : 1;
        m_frameRate = (options.frameRate == 30 || options.frameRate == 20) ? options.frameRate : 60;

        // 1. 入力フレームサイズと出力エンコードサイズを計算（H.264 エンコード用に解像度を偶数に揃える）
        uint32_t rawWidth = static_cast<uint32_t>(max(2, captureSize.Width));
        uint32_t rawHeight = static_cast<uint32_t>(max(2, captureSize.Height));
        m_sourceWidth = (rawWidth % 2 == 0) ? rawWidth : (rawWidth - 1);
        m_sourceHeight = (rawHeight % 2 == 0) ? rawHeight : (rawHeight - 1);
        if (m_sourceWidth < 2) m_sourceWidth = 2;
        if (m_sourceHeight < 2) m_sourceHeight = 2;

        uint32_t scaledWidth = max(2u, m_sourceWidth / divisor);
        uint32_t scaledHeight = max(2u, m_sourceHeight / divisor);
        m_encodeWidth = (scaledWidth % 2 == 0) ? scaledWidth : (scaledWidth - 1);
        m_encodeHeight = (scaledHeight % 2 == 0) ? scaledHeight : (scaledHeight - 1);
        if (m_encodeWidth < 2) m_encodeWidth = 2;
        if (m_encodeHeight < 2) m_encodeHeight = 2;

        // 2. 録画バッファ用の ID3D11Texture2D と IDirect3DSurface を作成（入力解像度）
        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);

            D3D11_TEXTURE2D_DESC texDesc{};
            texDesc.Width = m_sourceWidth;
            texDesc.Height = m_sourceHeight;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

            m_recordingTexture = nullptr;
            winrt::check_hresult(m_d3dHelper->Device()->CreateTexture2D(&texDesc, nullptr, m_recordingTexture.put()));

            auto dxgiSurface = m_recordingTexture.as<IDXGISurface>();
            com_ptr<::IInspectable> inspectableSurface;
            winrt::check_hresult(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), inspectableSurface.put()));
            m_recordingSurface = inspectableSurface.as<IDirect3DSurface>();

            m_hasNewFrame = false;
            m_startTime = TimeSpan::zero();
            m_lastFrameTime = TimeSpan::zero();
        }

        // 3. MediaStreamSource の構成 (入力はキャプチャ解像度、出力プロファイル側でハードウェア縮小)
        using namespace winrt::Windows::Media::Core;
        using namespace winrt::Windows::Media::MediaProperties;
        using namespace winrt::Windows::Media::Transcoding;

        auto videoProps = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Bgra8(), m_sourceWidth, m_sourceHeight);
        VideoStreamDescriptor videoDescriptor(videoProps);

        m_recordAudioEnabled = options.recordAudio && InitializeAudioCapture();

        if (m_recordAudioEnabled)
        {
            AudioEncodingProperties audioProps = AudioEncodingProperties::CreatePcm(m_audioSampleRate, m_audioChannels, m_audioBitsPerSample);
            if (m_audioIsFloat)
            {
                audioProps.Subtype(MediaEncodingSubtypes::Float());
            }
            AudioStreamDescriptor audioDescriptor(audioProps);
            m_mediaStreamSource = MediaStreamSource(videoDescriptor, audioDescriptor);
        }
        else
        {
            m_mediaStreamSource = MediaStreamSource(videoDescriptor);
        }

        m_mediaStreamSource.BufferTime(TimeSpan::zero());
        m_startingToken = m_mediaStreamSource.Starting({ this, &MediaRecorder::OnMediaStreamSourceStarting });
        m_sampleRequestedToken = m_mediaStreamSource.SampleRequested({ this, &MediaRecorder::OnMediaStreamSourceSampleRequested });

        // 4. MP4 エンコードプロファイルの設定 (指定サイズ・指定フレームレート + AAC音声)
        auto profile = MediaEncodingProfile::CreateMp4(VideoEncodingQuality::HD1080p);
        profile.Video().Width(m_encodeWidth);
        profile.Video().Height(m_encodeHeight);
        profile.Video().FrameRate().Numerator(m_frameRate);
        profile.Video().FrameRate().Denominator(1);
        if (!m_recordAudioEnabled)
        {
            profile.Audio(nullptr);
        }

        // 5. MediaTranscoder の準備と開始
        m_transcoder = MediaTranscoder();
        m_transcoder.HardwareAccelerationEnabled(true);

        m_isRecording = true;

        auto prepareOp = co_await m_transcoder.PrepareMediaStreamSourceTranscodeAsync(m_mediaStreamSource, m_outputStream, profile);
        if (prepareOp.CanTranscode())
        {
            m_transcodeOp = prepareOp.TranscodeAsync();
            co_return true;
        }
        else
        {
            StopAudioCapture();
            m_isRecording = false;
            co_return false;
        }
    }

    void MediaRecorder::ProcessFrame(ID3D11Texture2D* frameTexture, TimeSpan systemRelativeTime)
    {
        if (!m_isRecording || !frameTexture || !m_d3dHelper) return;

        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            if (!m_recordingTexture) return;

            D3D11_TEXTURE2D_DESC frameDesc{};
            frameTexture->GetDesc(&frameDesc);

            D3D11_BOX srcBox{};
            srcBox.left = 0;
            srcBox.top = 0;
            srcBox.front = 0;
            srcBox.right = min(m_sourceWidth, frameDesc.Width);
            srcBox.bottom = min(m_sourceHeight, frameDesc.Height);
            srcBox.back = 1;

            m_d3dHelper->Context()->CopySubresourceRegion(
                m_recordingTexture.get(), 0, 0, 0, 0,
                frameTexture, 0, &srcBox
            );
            m_d3dHelper->Context()->Flush();

            m_lastFrameTime = systemRelativeTime;
            m_hasNewFrame = true;
        }
        m_frameReadyCv.notify_all();
    }

    void MediaRecorder::OnMediaStreamSourceStarting(
        winrt::Windows::Media::Core::MediaStreamSource const&,
        winrt::Windows::Media::Core::MediaStreamSourceStartingEventArgs const& args)
    {
        auto request = args.Request();
        auto deferral = request.GetDeferral();

        {
            std::unique_lock<std::mutex> lock(m_recordingMutex);
            m_frameReadyCv.wait_for(lock, std::chrono::milliseconds(500), [this]() {
                return m_hasNewFrame || !m_isRecording.load();
            });
            m_startTime = m_lastFrameTime;
        }

        if (m_recordAudioEnabled)
        {
            StartAudioCaptureThread();
        }

        request.SetActualStartPosition(TimeSpan::zero());
        deferral.Complete();
    }

    void MediaRecorder::OnMediaStreamSourceSampleRequested(
        winrt::Windows::Media::Core::MediaStreamSource const&,
        winrt::Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const& args)
    {
        auto request = args.Request();
        if (!m_isRecording || !m_d3dHelper)
        {
            request.Sample(nullptr);
            return;
        }

        // 音声ストリームの要求か、映像ストリームの要求かを判定
        if (request.StreamDescriptor().try_as<winrt::Windows::Media::Core::AudioStreamDescriptor>())
        {
            auto deferral = request.GetDeferral();

            AudioChunk chunk;
            bool hasChunk = false;
            {
                std::unique_lock<std::mutex> lock(m_audioMutex);
                m_audioCv.wait_for(lock, std::chrono::milliseconds(50), [this]() {
                    return !m_audioQueue.empty() || !m_isRecording.load();
                });

                if (m_isRecording && !m_audioQueue.empty())
                {
                    chunk = std::move(m_audioQueue.front());
                    m_audioQueue.pop_front();
                    hasChunk = true;
                }
            }

            if (!m_isRecording)
            {
                request.Sample(nullptr);
                deferral.Complete();
                return;
            }

            if (!hasChunk)
            {
                uint32_t silentFrames = m_audioSampleRate / 100;
                chunk.data.assign(silentFrames * m_audioBlockAlign, 0);
                chunk.timestamp = TimeSpan{ static_cast<int64_t>((m_totalAudioFramesCaptured * 10000000ULL) / m_audioSampleRate) };
                chunk.duration = TimeSpan{ static_cast<int64_t>((silentFrames * 10000000ULL) / m_audioSampleRate) };
                m_totalAudioFramesCaptured += silentFrames;
            }

            auto ibuf = winrt::Windows::Security::Cryptography::CryptographicBuffer::CreateFromByteArray(chunk.data);
            auto sample = winrt::Windows::Media::Core::MediaStreamSample::CreateFromBuffer(ibuf, chunk.timestamp);
            sample.Duration(chunk.duration);
            request.Sample(sample);

            deferral.Complete();
            return;
        }

        // --- 映像ストリームのサンプル処理 ---
        auto deferral = request.GetDeferral();

        uint32_t waitMs = max(15u, 1000u / max(1u, m_frameRate));
        std::unique_lock<std::mutex> lock(m_recordingMutex);
        m_frameReadyCv.wait_for(lock, std::chrono::milliseconds(waitMs), [this]() {
            return m_hasNewFrame || !m_isRecording.load();
        });

        if (!m_isRecording || !m_recordingSurface)
        {
            request.Sample(nullptr);
            deferral.Complete();
            return;
        }

        m_hasNewFrame = false;
        auto timeStamp = m_lastFrameTime - m_startTime;
        if (timeStamp.count() < 0)
        {
            timeStamp = TimeSpan::zero();
        }

        D3D11_TEXTURE2D_DESC texDesc{};
        m_recordingTexture->GetDesc(&texDesc);
        com_ptr<ID3D11Texture2D> sampleTexture;
        if (SUCCEEDED(m_d3dHelper->Device()->CreateTexture2D(&texDesc, nullptr, sampleTexture.put())))
        {
            m_d3dHelper->Context()->CopyResource(sampleTexture.get(), m_recordingTexture.get());
            m_d3dHelper->Context()->Flush();

            auto dxgiSurface = sampleTexture.as<IDXGISurface>();
            com_ptr<::IInspectable> inspectableSurface;
            if (SUCCEEDED(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), inspectableSurface.put())))
            {
                auto sampleSurface = inspectableSurface.as<IDirect3DSurface>();
                auto sample = winrt::Windows::Media::Core::MediaStreamSample::CreateFromDirect3D11Surface(sampleSurface, timeStamp);
                request.Sample(sample);
            }
        }

        deferral.Complete();
    }

    IAsyncAction MediaRecorder::StopAsync()
    {
        if (!m_isRecording) co_return;

        m_isRecording = false;
        m_frameReadyCv.notify_all();
        m_audioCv.notify_all();

        StopAudioCapture();

        if (m_transcodeOp)
        {
            try
            {
                co_await m_transcodeOp;
            }
            catch (...)
            {
            }
            m_transcodeOp = nullptr;
        }

        if (m_mediaStreamSource)
        {
            m_mediaStreamSource.Starting(m_startingToken);
            m_mediaStreamSource.SampleRequested(m_sampleRequestedToken);
            m_mediaStreamSource = nullptr;
        }

        if (m_outputStream)
        {
            m_outputStream.Close();
            m_outputStream = nullptr;
        }

        m_transcoder = nullptr;

        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            m_recordingSurface = nullptr;
            m_recordingTexture = nullptr;
        }
    }

    // ============================================================================
    // WASAPI ループバック音声キャプチャ実装
    // ============================================================================

    bool MediaRecorder::InitializeAudioCapture()
    {
        StopAudioCapture();

        com_ptr<IMMDeviceEnumerator> enumerator;
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            IID_PPV_ARGS(enumerator.put())
        );
        if (FAILED(hr)) return false;

        com_ptr<IMMDevice> device;
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.put());
        if (FAILED(hr)) return false;

        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, m_audioClient.put_void());
        if (FAILED(hr)) return false;

        WAVEFORMATEX* pwfx = nullptr;
        hr = m_audioClient->GetMixFormat(&pwfx);
        if (FAILED(hr) || !pwfx) return false;

        m_audioSampleRate = pwfx->nSamplesPerSec;
        m_audioChannels = pwfx->nChannels;
        m_audioBitsPerSample = pwfx->wBitsPerSample;
        m_audioBlockAlign = pwfx->nBlockAlign;
        m_audioIsFloat = false;

        if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        {
            m_audioIsFloat = true;
        }
        else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && pwfx->cbSize >= 22)
        {
            auto ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx);
            if (IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
            {
                m_audioIsFloat = true;
            }
        }

        REFERENCE_TIME hnsRequestedDuration = 1000000;
        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            hnsRequestedDuration,
            0,
            pwfx,
            nullptr
        );
        CoTaskMemFree(pwfx);
        if (FAILED(hr))
        {
            m_audioClient = nullptr;
            return false;
        }

        hr = m_audioClient->GetService(IID_PPV_ARGS(m_audioCaptureClient.put()));
        if (FAILED(hr))
        {
            m_audioClient = nullptr;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(m_audioMutex);
            m_audioQueue.clear();
            m_totalAudioFramesCaptured = 0;
        }

        return true;
    }

    void MediaRecorder::StartAudioCaptureThread()
    {
        if (!m_audioClient || !m_audioCaptureClient || m_isAudioCapturing) return;

        if (FAILED(m_audioClient->Start())) return;

        m_isAudioCapturing = true;
        m_audioThread = std::thread(&MediaRecorder::AudioCaptureThreadProc, this);
    }

    void MediaRecorder::StopAudioCapture()
    {
        m_isAudioCapturing = false;
        m_audioCv.notify_all();

        if (m_audioThread.joinable())
        {
            m_audioThread.join();
        }

        if (m_audioClient)
        {
            m_audioClient->Stop();
        }

        m_audioCaptureClient = nullptr;
        m_audioClient = nullptr;

        {
            std::lock_guard<std::mutex> lock(m_audioMutex);
            m_audioQueue.clear();
        }
    }

    void MediaRecorder::AudioCaptureThreadProc()
    {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        auto lastPacketTick = std::chrono::steady_clock::now();

        while (m_isAudioCapturing && m_isRecording)
        {
            UINT32 packetLength = 0;
            HRESULT hr = m_audioCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) break;

            bool gotAnyPacket = false;

            while (packetLength != 0 && m_isAudioCapturing && m_isRecording)
            {
                BYTE* pData = nullptr;
                UINT32 numFramesAvailable = 0;
                DWORD flags = 0;

                hr = m_audioCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
                if (FAILED(hr)) break;

                if (numFramesAvailable > 0)
                {
                    gotAnyPacket = true;
                    size_t byteCount = static_cast<size_t>(numFramesAvailable) * m_audioBlockAlign;

                    AudioChunk chunk;
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    {
                        chunk.data.assign(byteCount, 0);
                    }
                    else
                    {
                        chunk.data.assign(pData, pData + byteCount);
                    }

                    {
                        std::lock_guard<std::mutex> lock(m_audioMutex);
                        chunk.timestamp = TimeSpan{ static_cast<int64_t>((m_totalAudioFramesCaptured * 10000000ULL) / m_audioSampleRate) };
                        chunk.duration = TimeSpan{ static_cast<int64_t>((numFramesAvailable * 10000000ULL) / m_audioSampleRate) };
                        m_totalAudioFramesCaptured += numFramesAvailable;
                        m_audioQueue.push_back(std::move(chunk));
                    }
                    m_audioCv.notify_one();
                }

                m_audioCaptureClient->ReleaseBuffer(numFramesAvailable);
                hr = m_audioCaptureClient->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) break;
            }

            auto now = std::chrono::steady_clock::now();
            if (gotAnyPacket)
            {
                lastPacketTick = now;
            }
            else
            {
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPacketTick).count();
                if (elapsedMs >= 20)
                {
                    lastPacketTick = now;
                    uint32_t silentFrames = (m_audioSampleRate * 20) / 1000;
                    size_t byteCount = static_cast<size_t>(silentFrames) * m_audioBlockAlign;

                    AudioChunk chunk;
                    chunk.data.assign(byteCount, 0);

                    {
                        std::lock_guard<std::mutex> lock(m_audioMutex);
                        chunk.timestamp = TimeSpan{ static_cast<int64_t>((m_totalAudioFramesCaptured * 10000000ULL) / m_audioSampleRate) };
                        chunk.duration = TimeSpan{ static_cast<int64_t>((silentFrames * 10000000ULL) / m_audioSampleRate) };
                        m_totalAudioFramesCaptured += silentFrames;
                        m_audioQueue.push_back(std::move(chunk));
                    }
                    m_audioCv.notify_one();
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        CoUninitialize();
    }
}
