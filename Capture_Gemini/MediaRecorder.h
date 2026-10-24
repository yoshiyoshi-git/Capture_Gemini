#pragma once
#include "Direct3DHelper.h"

namespace Capture_Gemini
{
    // 録画設定（画質・フレームレート・音声録音）
    struct RecordingOptions
    {
        uint32_t scaleDivisor{ 1 }; // 解像度の縮小率 (1 = 等倍, 2 = 1/2, 4 = 1/4)
        uint32_t frameRate{ 60 };   // フレームレート (60, 30, 20 fps)
        bool recordAudio{ true };   // システム音声を録音するか (true = 録音する)
    };

    // 映像とシステム音声(WASAPI)をエンコードして MP4 ファイルに保存するクラス
    class MediaRecorder
    {
    public:
        MediaRecorder();
        ~MediaRecorder();

        // MP4 録画・録音を非同期で開始する
        // 引数: d3dHelper = D3D管理クラス, outputStream = 保存先ストリーム,
        //       captureSize = 元映像サイズ, options = 録画設定
        // 戻り値: 開始成功なら true、失敗なら false (非同期)
        winrt::Windows::Foundation::IAsyncOperation<bool> StartAsync(
            Direct3DHelper& d3dHelper,
            winrt::Windows::Storage::Streams::IRandomAccessStream outputStream,
            winrt::Windows::Graphics::SizeInt32 captureSize,
            RecordingOptions options);

        // 録画・録音を停止し、MP4 ファイルの保存完了まで待機する
        // 引数: なし / 戻り値: 完了待機用の非同期アクション
        winrt::Windows::Foundation::IAsyncAction StopAsync();

        // キャプチャされた映像フレームを録画バッファへコピーする
        // 引数: frameTexture = フレームテクスチャ, systemRelativeTime = フレーム取得時刻 / 戻り値: なし
        void ProcessFrame(ID3D11Texture2D* frameTexture, winrt::Windows::Foundation::TimeSpan systemRelativeTime);

        // 録画中かどうかを返す (引数: なし / 戻り値: 録画中なら true)
        bool IsRecording() const noexcept { return m_isRecording.load(); }

    private:
        // エンコード開始時の初期化処理（基準時刻の記録と音声スレッド開始）
        // 引数: sender = イベント元, args = 開始イベント引数 / 戻り値: なし
        void OnMediaStreamSourceStarting(
            winrt::Windows::Media::Core::MediaStreamSource const& sender,
            winrt::Windows::Media::Core::MediaStreamSourceStartingEventArgs const& args);

        // エンコーダから映像フレームまたは音声サンプルを要求された時の供給処理
        // 引数: sender = イベント元, args = サンプル要求イベント引数 / 戻り値: なし
        void OnMediaStreamSourceSampleRequested(
            winrt::Windows::Media::Core::MediaStreamSource const& sender,
            winrt::Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const& args);

        // WASAPI ループバック音声キャプチャを初期化する (引数: なし / 戻り値: 成功なら true)
        bool InitializeAudioCapture();

        // 音声キャプチャ用スレッドを起動する (引数: なし / 戻り値: なし)
        void StartAudioCaptureThread();

        // 音声キャプチャ用スレッドを停止し、WASAPI リソースを解放する (引数: なし / 戻り値: なし)
        void StopAudioCapture();

        // 音声パケットを継続取得してキューに積むスレッド関数（無音時のゼロ埋め補填を含む）
        // 引数: なし / 戻り値: なし
        void AudioCaptureThreadProc();

        // 1パケット分の音声データ
        struct AudioChunk
        {
            std::vector<uint8_t> data;                        // PCM データ
            winrt::Windows::Foundation::TimeSpan timestamp;   // タイムスタンプ
            winrt::Windows::Foundation::TimeSpan duration;    // 再生時間長
        };

        Direct3DHelper* m_d3dHelper{ nullptr };               // D3D 管理クラス
        std::atomic<bool> m_isRecording{ false };             // 録画中フラグ
        std::mutex m_recordingMutex;                          // 映像フレーム同期用ミューテックス
        std::condition_variable m_frameReadyCv;               // 映像フレーム到着通知用の条件変数
        bool m_hasNewFrame{ false };                          // 新規フレーム有無フラグ

        uint32_t m_sourceWidth{ 0 };                          // 入力映像の幅（偶数）
        uint32_t m_sourceHeight{ 0 };                         // 入力映像の高さ（偶数）
        uint32_t m_encodeWidth{ 0 };                          // 出力 MP4 の幅（縮小後・偶数）
        uint32_t m_encodeHeight{ 0 };                         // 出力 MP4 の高さ（縮小後・偶数）
        uint32_t m_frameRate{ 60 };                           // 出力フレームレート

        winrt::com_ptr<ID3D11Texture2D> m_recordingTexture;                                         // 録画バッファ用テクスチャ
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface m_recordingSurface{ nullptr }; // WinRT サーフェス
        winrt::Windows::Foundation::TimeSpan m_startTime{ 0 };                                      // 録画開始時刻
        winrt::Windows::Foundation::TimeSpan m_lastFrameTime{ 0 };                                  // 最新フレーム時刻

        winrt::Windows::Media::Core::MediaStreamSource m_mediaStreamSource{ nullptr };              // ストリームソース
        winrt::Windows::Media::Transcoding::MediaTranscoder m_transcoder{ nullptr };                // MP4 トランスコーダ
        winrt::Windows::Storage::Streams::IRandomAccessStream m_outputStream{ nullptr };            // ファイル出力ストリーム
        winrt::Windows::Foundation::IAsyncActionWithProgress<double> m_transcodeOp{ nullptr };      // トランスコード非同期処理
        winrt::event_token m_startingToken{};                                                       // Starting トークン
        winrt::event_token m_sampleRequestedToken{};                                                // SampleRequested トークン

        bool m_recordAudioEnabled{ true };                    // 音声録音有効フラグ
        winrt::com_ptr<IAudioClient> m_audioClient;           // WASAPI オーディオクライアント
        winrt::com_ptr<IAudioCaptureClient> m_audioCaptureClient; // WASAPI キャプチャクライアント
        uint32_t m_audioSampleRate{ 48000 };                  // サンプリングレート (Hz)
        uint32_t m_audioChannels{ 2 };                        // チャンネル数
        uint32_t m_audioBitsPerSample{ 32 };                  // ビット深度
        uint32_t m_audioBlockAlign{ 8 };                      // 1フレームのバイト数
        bool m_audioIsFloat{ true };                          // Float 形式かどうか

        std::thread m_audioThread;                            // 音声取得スレッド
        std::atomic<bool> m_isAudioCapturing{ false };        // 音声取得中フラグ
        std::mutex m_audioMutex;                              // 音声キュー用ミューテックス
        std::condition_variable m_audioCv;                    // 音声キュー通知用の条件変数
        std::deque<AudioChunk> m_audioQueue;                  // 音声データ待ち行列
        uint64_t m_totalAudioFramesCaptured{ 0 };             // 累計取得オーディオフレーム数
    };
}
