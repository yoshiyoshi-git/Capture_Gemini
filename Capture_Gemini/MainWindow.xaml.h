#pragma once

#include "MainWindow.g.h"
#include "Direct3DHelper.h"
#include "ScreenCaptureSession.h"
#include "MediaRecorder.h"

namespace winrt::Capture_Gemini::implementation
{
    // メインウィンドウ（UI イベント処理と各クラスの連携を行う）
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

        // 「キャプチャ対象を選択」ボタン押下時（対象選択ピッカーを開いてキャプチャ開始）
        // 引数: sender = ボタン, e = イベント情報 / 戻り値: なし
        void StartCaptureButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // 「キャプチャ停止」ボタン押下時（キャプチャと録画を停止）
        // 引数: sender = ボタン, e = イベント情報 / 戻り値: なし
        void StopCaptureButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // 「録画開始」ボタン押下時（保存先ピッカーを開いて録画開始）
        // 引数: sender = ボタン, e = イベント情報 / 戻り値: なし
        void StartRecordButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // 「録画停止・保存」ボタン押下時（録画を停止してファイルを保存）
        // 引数: sender = ボタン, e = イベント情報 / 戻り値: なし
        void StopRecordButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // メニューの「画面サイズ」「フレームレート」変更時（録画設定を更新）
        // 引数: sender = メニュー項目, e = イベント情報 / 戻り値: なし
        void QualityMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // メニューの「システム音声も録音する」切り替え時（チェックボックスと同期）
        // 引数: sender = メニュー項目, e = イベント情報 / 戻り値: なし
        void RecordAudioMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // ツールバーの「システム音声も録音する」チェック変更時（メニューと同期）
        // 引数: sender = チェックボックス, e = イベント情報 / 戻り値: なし
        void RecordAudioCheckBox_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

    private:
        // このウィンドウの HWND を取得する（ピッカー表示の親ウィンドウ指定用）
        // 引数: なし / 戻り値: ウィンドウハンドル (HWND)
        HWND GetWindowHandle();

        // UI の選択状態を m_recordingOptions とステータス表示に反映する
        // 引数: なし / 戻り値: なし
        void UpdateQualitySettingsFromUI();

        // ピッカーでキャプチャ対象を選択し、キャプチャを開始する
        // 引数: なし / 戻り値: 非同期アクション
        winrt::Windows::Foundation::IAsyncAction StartCaptureAsync();

        // キャプチャを停止し、ボタン状態を戻す
        // 引数: なし / 戻り値: なし
        void StopCapture();

        // 保存先ファイルを選択し、MP4 録画を開始する
        // 引数: なし / 戻り値: 非同期アクション
        winrt::Windows::Foundation::IAsyncAction StartRecordAsync();

        // MP4 録画を停止し、保存完了まで待機する
        // 引数: なし / 戻り値: 非同期アクション
        winrt::Windows::Foundation::IAsyncAction StopRecordAsync();

        ::Capture_Gemini::Direct3DHelper m_d3dHelper;             // Direct3D・SwapChain 管理
        ::Capture_Gemini::ScreenCaptureSession m_captureSession;  // 画面キャプチャ管理
        ::Capture_Gemini::MediaRecorder m_mediaRecorder;          // MP4 録画・音声録音管理
        ::Capture_Gemini::RecordingOptions m_recordingOptions;    // 現在の録画設定
    };
}

namespace winrt::Capture_Gemini::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
