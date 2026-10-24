#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Capture;

namespace winrt::Capture_Gemini::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        m_d3dHelper.Initialize();
        UpdateQualitySettingsFromUI();
    }

    MainWindow::~MainWindow()
    {
        StopCapture();
    }

    HWND MainWindow::GetWindowHandle()
    {
        HWND hwnd = nullptr;
        auto windowNative = this->try_as<::IWindowNative>();
        if (windowNative)
        {
            windowNative->get_WindowHandle(&hwnd);
        }
        return hwnd;
    }

    void MainWindow::QualityMenuItem_Click(IInspectable const&, RoutedEventArgs const&)
    {
        UpdateQualitySettingsFromUI();
    }

    void MainWindow::RecordAudioMenuItem_Click(IInspectable const&, RoutedEventArgs const&)
    {
        bool checked = RecordAudioMenuItem().IsChecked();
        RecordAudioCheckBox().IsChecked(checked);
        m_recordingOptions.recordAudio = checked;
    }

    void MainWindow::RecordAudioCheckBox_Click(IInspectable const&, RoutedEventArgs const&)
    {
        bool checked = RecordAudioCheckBox().IsChecked().GetBoolean();
        RecordAudioMenuItem().IsChecked(checked);
        m_recordingOptions.recordAudio = checked;
    }

    void MainWindow::UpdateQualitySettingsFromUI()
    {
        if (ScaleHalfMenuItem().IsChecked())
        {
            m_recordingOptions.scaleDivisor = 2;
        }
        else if (ScaleQuarterMenuItem().IsChecked())
        {
            m_recordingOptions.scaleDivisor = 4;
        }
        else
        {
            m_recordingOptions.scaleDivisor = 1;
        }

        if (Fps30MenuItem().IsChecked())
        {
            m_recordingOptions.frameRate = 30;
        }
        else if (Fps20MenuItem().IsChecked())
        {
            m_recordingOptions.frameRate = 20;
        }
        else
        {
            m_recordingOptions.frameRate = 60;
        }

        m_recordingOptions.recordAudio = RecordAudioCheckBox().IsChecked().GetBoolean();

        std::wstring scaleText = (m_recordingOptions.scaleDivisor == 1)
            ? L"等倍"
            : (L"1/" + std::to_wstring(m_recordingOptions.scaleDivisor));
        std::wstring statusText = L"録画設定: " + scaleText + L" / " + std::to_wstring(m_recordingOptions.frameRate) + L" fps";
        QualityStatusTextBlock().Text(statusText);
    }

    void MainWindow::StartCaptureButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        StartCaptureAsync();
    }

    void MainWindow::StopCaptureButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        StopCapture();
    }

    void MainWindow::StartRecordButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        StartRecordAsync();
    }

    void MainWindow::StopRecordButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        StopRecordAsync();
    }

    IAsyncAction MainWindow::StartCaptureAsync()
    {
        GraphicsCapturePicker picker;
        auto initWindow = picker.as<::IInitializeWithWindow>();
        winrt::check_hresult(initWindow->Initialize(GetWindowHandle()));

        GraphicsCaptureItem item = co_await picker.PickSingleItemAsync();
        if (!item) co_return;

        StopCapture();

        m_captureSession.StartCapture(
            m_d3dHelper,
            item,
            PreviewSwapChainPanel(),
            [this](ID3D11Texture2D* frameTexture, TimeSpan systemRelativeTime)
            {
                m_mediaRecorder.ProcessFrame(frameTexture, systemRelativeTime);
            },
            [this]()
            {
                DispatcherQueue().TryEnqueue([this]()
                {
                    StopCapture();
                });
            }
        );

        StartCaptureButton().IsEnabled(false);
        StopCaptureButton().IsEnabled(true);
        StartRecordButton().IsEnabled(true);
    }

    void MainWindow::StopCapture()
    {
        if (m_mediaRecorder.IsRecording())
        {
            StopRecordAsync();
        }

        m_captureSession.StopCapture(m_d3dHelper, PreviewSwapChainPanel());

        StartCaptureButton().IsEnabled(true);
        StopCaptureButton().IsEnabled(false);
        StartRecordButton().IsEnabled(false);
        StopRecordButton().IsEnabled(false);
    }

    IAsyncAction MainWindow::StartRecordAsync()
    {
        if (!m_captureSession.IsCapturing() || m_mediaRecorder.IsRecording()) co_return;

        UpdateQualitySettingsFromUI();

        winrt::Windows::Storage::Pickers::FileSavePicker savePicker;
        auto initWindow = savePicker.as<::IInitializeWithWindow>();
        winrt::check_hresult(initWindow->Initialize(GetWindowHandle()));

        savePicker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::VideosLibrary);
        savePicker.FileTypeChoices().Insert(L"MP4 Video", winrt::single_threaded_vector<hstring>({ L".mp4" }));
        savePicker.SuggestedFileName(L"RecordedVideo");

        winrt::Windows::Storage::StorageFile file = co_await savePicker.PickSaveFileAsync();
        if (!file) co_return;

        auto outputStream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite);

        SettingsMenuBarItem().IsEnabled(false);
        RecordAudioCheckBox().IsEnabled(false);
        StartRecordButton().IsEnabled(false);
        StopRecordButton().IsEnabled(true);

        bool started = co_await m_mediaRecorder.StartAsync(
            m_d3dHelper,
            outputStream,
            m_captureSession.CurrentSize(),
            m_recordingOptions
        );

        if (!started)
        {
            SettingsMenuBarItem().IsEnabled(true);
            RecordAudioCheckBox().IsEnabled(true);
            StartRecordButton().IsEnabled(true);
            StopRecordButton().IsEnabled(false);
        }
    }

    IAsyncAction MainWindow::StopRecordAsync()
    {
        if (!m_mediaRecorder.IsRecording()) co_return;

        StopRecordButton().IsEnabled(false);

        co_await m_mediaRecorder.StopAsync();

        SettingsMenuBarItem().IsEnabled(true);
        RecordAudioCheckBox().IsEnabled(true);
        StartRecordButton().IsEnabled(m_captureSession.IsCapturing());
    }
}
