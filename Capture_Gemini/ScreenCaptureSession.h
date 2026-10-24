#pragma once
#include "Direct3DHelper.h"

namespace Capture_Gemini
{
    // ウィンドウや画面のキャプチャとプレビュー更新を管理するクラス
    class ScreenCaptureSession
    {
    public:
        // フレーム到着時コールバック (引数: フレームテクスチャ, 取得時刻 / 戻り値: なし)
        using FrameCallback = std::function<void(ID3D11Texture2D*, winrt::Windows::Foundation::TimeSpan)>;

        // キャプチャ対象ウィンドウ終了時コールバック (引数: なし / 戻り値: なし)
        using ClosedCallback = std::function<void()>;

        ScreenCaptureSession();
        ~ScreenCaptureSession();

        // 画面キャプチャとパネルへのプレビュー表示を開始する
        // 引数: d3dHelper = D3D管理クラス, item = キャプチャ対象, previewPanel = 表示先パネル,
        //       onFrameCallback = フレーム到着時処理, onClosedCallback = 対象終了時処理
        // 戻り値: なし
        void StartCapture(
            Direct3DHelper& d3dHelper,
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item,
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& previewPanel,
            FrameCallback onFrameCallback,
            ClosedCallback onClosedCallback);

        // 画面キャプチャを停止し、リソースと SwapChain を解放する
        // 引数: d3dHelper = D3D管理クラス, previewPanel = 表示先パネル / 戻り値: なし
        void StopCapture(
            Direct3DHelper& d3dHelper,
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& previewPanel);

        // キャプチャ中かどうかを返す (引数: なし / 戻り値: キャプチャ中なら true)
        bool IsCapturing() const noexcept { return m_captureItem != nullptr; }

        // 現在のキャプチャサイズを返す (引数: なし / 戻り値: 幅・高さ)
        winrt::Windows::Graphics::SizeInt32 CurrentSize() const noexcept { return m_lastSize; }

    private:
        // 新しいフレームが到着した時の処理（プレビュー描画と録画コールバック呼び出し）
        // 引数: sender = フレームプール, args = イベント引数 / 戻り値: なし
        void OnFrameArrived(
            winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
            winrt::Windows::Foundation::IInspectable const& args);

        Direct3DHelper* m_d3dHelper{ nullptr };                                                 // D3D 管理クラスへのポインタ
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };        // キャプチャ対象
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };   // フレームプール
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };         // キャプチャセッション
        winrt::Windows::Graphics::SizeInt32 m_lastSize{ 0, 0 };                                 // 直近のキャプチャサイズ
        winrt::event_token m_frameArrivedToken{};                                               // FrameArrived イベントトークン
        winrt::event_token m_closedToken{};                                                     // Closed イベントトークン

        FrameCallback m_onFrameCallback;                                                        // フレーム到着時コールバック
        ClosedCallback m_onClosedCallback;                                                      // 対象終了時コールバック
    };
}
