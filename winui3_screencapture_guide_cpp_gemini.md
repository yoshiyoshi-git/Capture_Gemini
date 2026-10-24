# WinUI 3 (C++/WinRT) 画面キャプチャ・録画アプリ 完全実装ガイド

## 概要

このドキュメントは、**WinUI 3 (Windows App SDK)** と **C++/WinRT API** を使用して、他のアプリケーションのウィンドウや画面をキャプチャし、リアルタイムでアプリ内にプレビュー表示（フェーズ1）し、さらにMP4動画として録画・保存（フェーズ2）するアプリケーションの完全な実装仕様書です。

AIエージェント（Antigravity等）は、ユーザーの指示に従い「フェーズ1」または「フェーズ2」のコードをプロジェクトに適用してください。

---

## 技術スタック・使用API

* **言語・フレームワーク**: C++ (C++20) / C++/WinRT / WinUI 3 (Windows App SDK)
* **画面キャプチャ**: `winrt::Windows::Graphics::Capture` (`GraphicsCapturePicker`, `Direct3D11CaptureFramePool`, `GraphicsCaptureSession`)
* **DirectX / COM Interop**:
  * Direct3D 11 (`ID3D11Device`, `ID3D11DeviceContext`, `IDXGISwapChain1`, `ID3D11Texture2D`)
  * `ISwapChainPanelNative` (`<microsoft.ui.xaml.media.dxinterop.h>`)
  * `IDirect3DDxgiInterfaceAccess` (`<windows.graphics.directx.direct3d11.interop.h>`)
  * `IInitializeWithWindow` (`<shobjidl_core.h>`)
  * `IWindowNative` (`<microsoft.ui.xaml.window.h>`)
* **動画エンコード・保存 (フェーズ2)**:
  * `winrt::Windows::Media::Core::MediaStreamSource`, `MediaStreamSample`
  * `winrt::Windows::Media::Transcoding::MediaTranscoder`
  * `winrt::Windows::Media::MediaProperties::MediaEncodingProfile`

---

## 事前準備 (プロジェクト設定・注意事項)

1. **プロジェクトテンプレート**
   * Visual Studio で「空白のアプリ、パック済み (WinUI 3 in Desktop)」(C++) を作成します。
2. **名前空間の置換ルール (AIへの重要指示)**
   * 本ドキュメントのコード例ではプロジェクトのルート名前空間を `ScreenCaptureAppCpp` としています。**実装時は必ず現在のプロジェクトの実際の名前空間（`.vcxproj` や既存の `MainWindow.h` に記載されている名前空間）に置き換えてください。**
3. **リンクライブラリ**
   * `d3d11.lib`, `dxgi.lib` をリンクする必要があります（コード内の `#pragma comment(lib, ...)` で自動指定済み）。

---

## フェーズ 1: 他のアプリをキャプチャしてアプリ内に表示する

### 1.1 UIの構築 (`MainWindow.xaml`)

キャプチャ操作用のボタンと、Direct3DのSwapChainを描画するための `SwapChainPanel` を配置します。

```xml
<Window
    x:Class="ScreenCaptureAppCpp.MainWindow"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
    xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
    mc:Ignorable="d"
    Title="WinUI 3 Screen Recorder (C++/WinRT)">

    <Grid RowDefinitions="Auto, *">
        <!-- 操作パネル -->
        <StackPanel Grid.Row="0" Orientation="Horizontal" Spacing="12" Padding="16" Background="{ThemeResource SurfaceStrokeColorDefaultBrush}">
            <Button x:Name="StartCaptureButton" Content="キャプチャ対象を選択" Click="StartCaptureButton_Click" />
            <Button x:Name="StopCaptureButton" Content="キャプチャ停止" Click="StopCaptureButton_Click" IsEnabled="False" />
            
            <!-- フェーズ2で使用する録画ボタン -->
            <Button x:Name="StartRecordButton" Content="録画開始" Click="StartRecordButton_Click" IsEnabled="False" Background="Red" Foreground="White" />
            <Button x:Name="StopRecordButton" Content="録画停止・保存" Click="StopRecordButton_Click" IsEnabled="False" />
        </StackPanel>

        <!-- プレビュー表示領域 -->
        <Border Grid.Row="1" Margin="16" BorderBrush="Gray" BorderThickness="1">
            <SwapChainPanel x:Name="PreviewSwapChainPanel" />
        </Border>
    </Grid>
</Window>
```

### 1.2 プリコンパイル済みヘッダーの更新 (`pch.h`)

既存の `pch.h` に、キャプチャ・DirectX相互運用・メディアエンコードに必要なヘッダーを追加します。

```cpp
#pragma once
#include <windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>

// Undefine GetCurrentTime macro to prevent
// conflict with Storyboard::GetCurrentTime
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <wil/cppwinrt_helpers.h>

// --- 画面キャプチャ・DirectX・録画用 追加ヘッダー ---
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.MediaProperties.h>
#include <winrt/Windows.Media.Transcoding.h>

#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <shobjidl_core.h>
#include <microsoft.ui.xaml.window.h>
#include <microsoft.ui.xaml.media.dxinterop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <mutex>
#include <atomic>
#include <condition_variable>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
```

### 1.3 ヘッダー定義 (`MainWindow.xaml.h`)

フェーズ1のプレビュー機能と、フェーズ2の録画機能の両方のメンバ変数を定義します。

```cpp
#pragma once

#include "MainWindow.g.h"

namespace winrt::ScreenCaptureAppCpp::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

        void StartCaptureButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void StopCaptureButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void StartRecordButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void StopRecordButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

    private:
        // フェーズ1: D3D初期化・キャプチャ・プレビュー関連
        void InitializeDirect3D();
        HWND GetWindowHandle();
        winrt::Windows::Foundation::IAsyncAction StartCaptureAsync();
        void StopCapture();
        void OnFrameArrived(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const& args);

        winrt::com_ptr<ID3D11Device> m_d3dDevice;
        winrt::com_ptr<ID3D11DeviceContext> m_d3dContext;
        winrt::com_ptr<IDXGISwapChain1> m_swapChain;
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_winrtD3dDevice{ nullptr };

        winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
        winrt::Windows::Graphics::SizeInt32 m_lastSize{ 0, 0 };
        winrt::event_token m_frameArrivedToken{};
        winrt::event_token m_closedToken{};

        // フェーズ2: MP4録画・保存関連
        winrt::Windows::Foundation::IAsyncAction StartRecordAsync();
        winrt::Windows::Foundation::IAsyncAction StopRecordAsync();
        void ProcessFrameForRecording(ID3D11Texture2D* frameTexture, winrt::Windows::Foundation::TimeSpan systemRelativeTime);
        void OnMediaStreamSourceStarting(winrt::Windows::Media::Core::MediaStreamSource const& sender, winrt::Windows::Media::Core::MediaStreamSourceStartingEventArgs const& args);
        void OnMediaStreamSourceSampleRequested(winrt::Windows::Media::Core::MediaStreamSource const& sender, winrt::Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const& args);

        std::atomic<bool> m_isRecording{ false };
        std::mutex m_recordingMutex;
        std::condition_variable m_frameReadyCv;
        bool m_hasNewFrame{ false };

        uint32_t m_recordWidth{ 0 };
        uint32_t m_recordHeight{ 0 };
        winrt::com_ptr<ID3D11Texture2D> m_recordingTexture;
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface m_recordingSurface{ nullptr };
        winrt::Windows::Foundation::TimeSpan m_startTime{ 0 };
        winrt::Windows::Foundation::TimeSpan m_lastFrameTime{ 0 };

        winrt::Windows::Media::Core::MediaStreamSource m_mediaStreamSource{ nullptr };
        winrt::Windows::Media::Transcoding::MediaTranscoder m_transcoder{ nullptr };
        winrt::Windows::Storage::Streams::IRandomAccessStream m_outputStream{ nullptr };
        winrt::Windows::Foundation::IAsyncAction m_transcodeOp{ nullptr };
        winrt::event_token m_startingToken{};
        winrt::event_token m_sampleRequestedToken{};
    };
}

namespace winrt::ScreenCaptureAppCpp::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
```

### 1.4 実装ファイル (`MainWindow.xaml.cpp`) - フェーズ1部分

まずはアプリ内にキャプチャ映像を表示するフェーズ1の実装です（録画ボタンのイベントハンドラは空のスタブにしてあります）。

```cpp
#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Windows::Foundation;
using namespace Windows::Graphics;
using namespace Windows::Graphics::Capture;
using namespace Windows::Graphics::DirectX;
using namespace Windows::Graphics::DirectX::Direct3D11;

namespace winrt::ScreenCaptureAppCpp::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        InitializeDirect3D();
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

    void MainWindow::InitializeDirect3D()
    {
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0
        };

        winrt::check_hresult(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            creationFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            m_d3dDevice.put(),
            nullptr,
            m_d3dContext.put()
        ));

        // マルチスレッド保護を有効化 (FramePoolおよびMediaStreamSourceの別スレッドアクセス対策)
        if (auto multithread = m_d3dDevice.try_as<ID3D10Multithread>())
        {
            multithread->SetMultithreadProtected(TRUE);
        }

        // ID3D11Device から WinRT の IDirect3DDevice を生成
        auto dxgiDevice = m_d3dDevice.as<IDXGIDevice>();
        com_ptr<::IInspectable> inspectableDevice;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectableDevice.put()));
        m_winrtD3dDevice = inspectableDevice.as<IDirect3DDevice>();
    }

    void MainWindow::StartCaptureButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        StartCaptureAsync();
    }

    void MainWindow::StopCaptureButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        StopCapture();
    }

    IAsyncAction MainWindow::StartCaptureAsync()
    {
        GraphicsCapturePicker picker;
        auto initWindow = picker.as<::IInitializeWithWindow>();
        winrt::check_hresult(initWindow->Initialize(GetWindowHandle()));

        GraphicsCaptureItem item = co_await picker.PickSingleItemAsync();
        if (!item) co_return;

        StopCapture();

        m_captureItem = item;
        m_lastSize = m_captureItem.Size();

        // 対象ウィンドウが閉じられたらキャプチャを自動停止
        m_closedToken = m_captureItem.Closed([this](auto&&, auto&&)
        {
            DispatcherQueue().TryEnqueue([this]()
            {
                StopCapture();
            });
        });

        // DXGI SwapChain の作成
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = static_cast<UINT>(max(1, m_lastSize.Width));
        desc.Height = static_cast<UINT>(max(1, m_lastSize.Height));
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Stereo = FALSE;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

        auto dxgiDevice = m_d3dDevice.as<IDXGIDevice2>();
        com_ptr<IDXGIAdapter> dxgiAdapter;
        winrt::check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));
        com_ptr<IDXGIFactory2> dxgiFactory;
        winrt::check_hresult(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.put())));

        winrt::check_hresult(dxgiFactory->CreateSwapChainForComposition(
            m_d3dDevice.get(),
            &desc,
            nullptr,
            m_swapChain.put()
        ));

        // SwapChainPanel に SwapChain をセット
        auto panelNative = PreviewSwapChainPanel().as<::ISwapChainPanelNative>();
        winrt::check_hresult(panelNative->SetSwapChain(m_swapChain.get()));

        // FramePool の作成 (バックグラウンドスレッドで動作する CreateFreeThreaded を使用)
        m_framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(
            m_winrtD3dDevice,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            m_lastSize
        );

        m_frameArrivedToken = m_framePool.FrameArrived({ this, &MainWindow::OnFrameArrived });

        m_session = m_framePool.CreateCaptureSession(m_captureItem);
        m_session.StartCapture();

        StartCaptureButton().IsEnabled(false);
        StopCaptureButton().IsEnabled(true);
        StartRecordButton().IsEnabled(true);
    }

    void MainWindow::OnFrameArrived(Direct3D11CaptureFramePool const& sender, IInspectable const&)
    {
        auto frame = sender.TryGetNextFrame();
        if (!frame) return;

        auto contentSize = frame.ContentSize();
        if (contentSize.Width <= 0 || contentSize.Height <= 0) return;

        bool needRecreate = (contentSize.Width != m_lastSize.Width || contentSize.Height != m_lastSize.Height);
        if (needRecreate)
        {
            m_lastSize = contentSize;
            m_swapChain->ResizeBuffers(
                2,
                static_cast<UINT>(m_lastSize.Width),
                static_cast<UINT>(m_lastSize.Height),
                DXGI_FORMAT_B8G8R8A8_UNORM,
                0
            );
        }

        // フレームの IDirect3DSurface からネイティブの ID3D11Texture2D を取得
        auto surface = frame.Surface();
        auto access = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        com_ptr<ID3D11Texture2D> frameTexture;
        winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(frameTexture.put())));

        // SwapChain のバックバッファを取得してコピー
        com_ptr<ID3D11Texture2D> backBuffer;
        winrt::check_hresult(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.put())));

        D3D11_TEXTURE2D_DESC backDesc{};
        backBuffer->GetDesc(&backDesc);
        D3D11_TEXTURE2D_DESC frameDesc{};
        frameTexture->GetDesc(&frameDesc);

        D3D11_BOX sourceRegion{};
        sourceRegion.left = 0;
        sourceRegion.top = 0;
        sourceRegion.front = 0;
        sourceRegion.right = min(backDesc.Width, frameDesc.Width);
        sourceRegion.bottom = min(backDesc.Height, frameDesc.Height);
        sourceRegion.back = 1;

        m_d3dContext->CopySubresourceRegion(backBuffer.get(), 0, 0, 0, 0, frameTexture.get(), 0, &sourceRegion);

        DXGI_PRESENT_PARAMETERS presentParams{};
        m_swapChain->Present1(1, 0, &presentParams);

        // 【フェーズ2用フック】録画中ならフレームを録画バッファへコピー
        ProcessFrameForRecording(frameTexture.get(), frame.SystemRelativeTime());

        if (needRecreate)
        {
            m_framePool.Recreate(
                m_winrtD3dDevice,
                DirectXPixelFormat::B8G8R8A8UIntNormalized,
                2,
                m_lastSize
            );
        }
    }

    void MainWindow::StopCapture()
    {
        if (m_isRecording)
        {
            StopRecordAsync();
        }

        if (m_session)
        {
            m_session.Close();
            m_session = nullptr;
        }
        if (m_framePool)
        {
            m_framePool.FrameArrived(m_frameArrivedToken);
            m_framePool.Close();
            m_framePool = nullptr;
        }
        if (m_captureItem)
        {
            m_captureItem.Closed(m_closedToken);
            m_captureItem = nullptr;
        }
        if (m_swapChain)
        {
            auto panelNative = PreviewSwapChainPanel().as<::ISwapChainPanelNative>();
            panelNative->SetSwapChain(nullptr);
            m_swapChain = nullptr;
        }

        StartCaptureButton().IsEnabled(true);
        StopCaptureButton().IsEnabled(false);
        StartRecordButton().IsEnabled(false);
        StopRecordButton().IsEnabled(false);
    }

    // --- フェーズ1時点では以下のメソッドを空スタブとして定義（フェーズ2で実装に置き換える） ---
    void MainWindow::ProcessFrameForRecording(ID3D11Texture2D*, TimeSpan) {}
    void MainWindow::StartRecordButton_Click(IInspectable const&, RoutedEventArgs const&) {}
    void MainWindow::StopRecordButton_Click(IInspectable const&, RoutedEventArgs const&) {}
    IAsyncAction MainWindow::StartRecordAsync() { co_return; }
    IAsyncAction MainWindow::StopRecordAsync() { co_return; }
    void MainWindow::OnMediaStreamSourceStarting(Windows::Media::Core::MediaStreamSource const&, Windows::Media::Core::MediaStreamSourceStartingEventArgs const&) {}
    void MainWindow::OnMediaStreamSourceSampleRequested(Windows::Media::Core::MediaStreamSource const&, Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const&) {}
}
```

---

## フェーズ 2: キャプチャした映像を録画してMP4ファイルに保存する

フェーズ1でプレビュー表示が正常に動作したら、`MainWindow.xaml.cpp` の末尾にあるスタブメソッド群を以下の完全な録画・エンコード実装に置き換えます。

### 2.1 録画アーキテクチャのポイント (C++/WinRT)
1. **偶数解像度の保証**: H.264 エンコーダの仕様上、幅と高さは2の倍数（偶数）である必要があります。
2. **D3D11テクスチャのラップ**: 録画フレーム保持用の `ID3D11Texture2D` を作成し、`CreateDirect3D11SurfaceFromDXGISurface` API を使って WinRT の `IDirect3DSurface` に変換し、`MediaStreamSample::CreateFromDirect3D11Surface` に渡します。
3. **スレッド同期**: `std::mutex` と `std::condition_variable` を使い、キャプチャスレッド（`OnFrameArrived`）とエンコーダスレッド（`SampleRequested`）の間で安全にフレームを受け渡します。

### 2.2 録画・保存機能の実装コード (`MainWindow.xaml.cpp` のスタブ置換用)

```cpp
    // ============================================================================
    // フェーズ2: 以下のコードで MainWindow.xaml.cpp 末尾のスタブ関数群を置き換える
    // ============================================================================

    void MainWindow::StartRecordButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        StartRecordAsync();
    }

    void MainWindow::StopRecordButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        StopRecordAsync();
    }

    IAsyncAction MainWindow::StartRecordAsync()
    {
        if (!m_captureItem || m_isRecording) co_return;

        // 1. 保存先ファイルを FileSavePicker で選択
        Windows::Storage::Pickers::FileSavePicker savePicker;
        auto initWindow = savePicker.as<::IInitializeWithWindow>();
        winrt::check_hresult(initWindow->Initialize(GetWindowHandle()));

        savePicker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::VideosLibrary);
        savePicker.FileTypeChoices().Insert(L"MP4 Video", winrt::single_threaded_vector<hstring>({ L".mp4" }));
        savePicker.SuggestedFileName(L"RecordedVideo");

        Windows::Storage::StorageFile file = co_await savePicker.PickSaveFileAsync();
        if (!file) co_return;

        m_outputStream = co_await file.OpenAsync(Windows::Storage::FileAccessMode::ReadWrite);

        // 2. H.264 エンコード用に解像度を偶数に揃える
        m_recordWidth = static_cast<uint32_t>(m_lastSize.Width % 2 == 0 ? m_lastSize.Width : m_lastSize.Width - 1);
        m_recordHeight = static_cast<uint32_t>(m_lastSize.Height % 2 == 0 ? m_lastSize.Height : m_lastSize.Height - 1);
        if (m_recordWidth < 2) m_recordWidth = 2;
        if (m_recordHeight < 2) m_recordHeight = 2;

        // 3. 録画バッファ用の ID3D11Texture2D と IDirect3DSurface を作成
        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);

            D3D11_TEXTURE2D_DESC texDesc{};
            texDesc.Width = m_recordWidth;
            texDesc.Height = m_recordHeight;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

            m_recordingTexture = nullptr;
            winrt::check_hresult(m_d3dDevice->CreateTexture2D(&texDesc, nullptr, m_recordingTexture.put()));

            auto dxgiSurface = m_recordingTexture.as<IDXGISurface>();
            com_ptr<::IInspectable> inspectableSurface;
            winrt::check_hresult(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), inspectableSurface.put()));
            m_recordingSurface = inspectableSurface.as<IDirect3DSurface>();

            m_hasNewFrame = false;
            m_startTime = TimeSpan::zero();
            m_lastFrameTime = TimeSpan::zero();
        }

        // 4. MediaStreamSource の構成
        using namespace Windows::Media::Core;
        using namespace Windows::Media::MediaProperties;
        using namespace Windows::Media::Transcoding;

        auto videoProps = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Bgra8(), m_recordWidth, m_recordHeight);
        VideoStreamDescriptor videoDescriptor(videoProps);

        m_mediaStreamSource = MediaStreamSource(videoDescriptor);
        m_mediaStreamSource.BufferTime(TimeSpan::zero());
        m_startingToken = m_mediaStreamSource.Starting({ this, &MainWindow::OnMediaStreamSourceStarting });
        m_sampleRequestedToken = m_mediaStreamSource.SampleRequested({ this, &MainWindow::OnMediaStreamSourceSampleRequested });

        // 5. MP4 エンコードプロファイルの設定 (60fps)
        auto profile = MediaEncodingProfile::CreateMp4(VideoEncodingQuality::HD1080p);
        profile.Video().Width(m_recordWidth);
        profile.Video().Height(m_recordHeight);
        profile.Video().FrameRate().Numerator(60);
        profile.Video().FrameRate().Denominator(1);
        profile.Audio(nullptr); // 映像のみ

        // 6. MediaTranscoder の準備と開始
        m_transcoder = MediaTranscoder();
        m_transcoder.HardwareAccelerationEnabled(true);

        m_isRecording = true;
        StartRecordButton().IsEnabled(false);
        StopRecordButton().IsEnabled(true);

        auto prepareOp = co_await m_transcoder.PrepareMediaStreamSourceTranscodeAsync(m_mediaStreamSource, m_outputStream, profile);
        if (prepareOp.CanTranscode())
        {
            m_transcodeOp = prepareOp.TranscodeAsync();
        }
        else
        {
            m_isRecording = false;
            StartRecordButton().IsEnabled(true);
            StopRecordButton().IsEnabled(false);
        }
    }

    void MainWindow::ProcessFrameForRecording(ID3D11Texture2D* frameTexture, TimeSpan systemRelativeTime)
    {
        if (!m_isRecording || !frameTexture) return;

        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            if (!m_recordingTexture) return;

            D3D11_TEXTURE2D_DESC frameDesc{};
            frameTexture->GetDesc(&frameDesc);

            // 録画サイズからはみ出さないように領域を計算してコピー
            D3D11_BOX srcBox{};
            srcBox.left = 0;
            srcBox.top = 0;
            srcBox.front = 0;
            srcBox.right = min(m_recordWidth, frameDesc.Width);
            srcBox.bottom = min(m_recordHeight, frameDesc.Height);
            srcBox.back = 1;

            m_d3dContext->CopySubresourceRegion(
                m_recordingTexture.get(), 0, 0, 0, 0,
                frameTexture, 0, &srcBox
            );
            m_d3dContext->Flush();

            m_lastFrameTime = systemRelativeTime;
            m_hasNewFrame = true;
        }
        m_frameReadyCv.notify_all();
    }

    void MainWindow::OnMediaStreamSourceStarting(
        Windows::Media::Core::MediaStreamSource const&,
        Windows::Media::Core::MediaStreamSourceStartingEventArgs const& args)
    {
        auto request = args.Request();
        auto deferral = request.GetDeferral();

        std::unique_lock<std::mutex> lock(m_recordingMutex);
        m_frameReadyCv.wait_for(lock, std::chrono::milliseconds(500), [this]() {
            return m_hasNewFrame || !m_isRecording.load();
        });

        m_startTime = m_lastFrameTime;
        request.SetActualStartPosition(TimeSpan::zero());
        deferral.Complete();
    }

    void MainWindow::OnMediaStreamSourceSampleRequested(
        Windows::Media::Core::MediaStreamSource const&,
        Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const& args)
    {
        auto request = args.Request();
        if (!m_isRecording)
        {
            // null を設定することでストリーム終端 (EOS) を通知し MP4 のフッターを書き込ませる
            request.Sample(nullptr);
            return;
        }

        auto deferral = request.GetDeferral();

        std::unique_lock<std::mutex> lock(m_recordingMutex);
        // 新しいフレームが来るまで最大33ms待機（動きがない画面でも約30fpsで直前フレームを送出）
        m_frameReadyCv.wait_for(lock, std::chrono::milliseconds(33), [this]() {
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

        // MediaTranscoder が読み取るため、独立したテクスチャに複製して Sample を作成
        D3D11_TEXTURE2D_DESC texDesc{};
        m_recordingTexture->GetDesc(&texDesc);
        com_ptr<ID3D11Texture2D> sampleTexture;
        if (SUCCEEDED(m_d3dDevice->CreateTexture2D(&texDesc, nullptr, sampleTexture.put())))
        {
            m_d3dContext->CopyResource(sampleTexture.get(), m_recordingTexture.get());
            m_d3dContext->Flush();

            auto dxgiSurface = sampleTexture.as<IDXGISurface>();
            com_ptr<::IInspectable> inspectableSurface;
            if (SUCCEEDED(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), inspectableSurface.put())))
            {
                auto sampleSurface = inspectableSurface.as<IDirect3DSurface>();
                auto sample = Windows::Media::Core::MediaStreamSample::CreateFromDirect3D11Surface(sampleSurface, timeStamp);
                request.Sample(sample);
            }
        }

        deferral.Complete();
    }

    IAsyncAction MainWindow::StopRecordAsync()
    {
        if (!m_isRecording) co_return;

        StopRecordButton().IsEnabled(false);
        m_isRecording = false;
        m_frameReadyCv.notify_all();

        if (m_transcodeOp)
        {
            try
            {
                // MP4ファイルのファイナライズ完了を待機
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

        StartRecordButton().IsEnabled(m_captureItem != nullptr);
    }
```

---

## Antigravity へのプロンプト指示テンプレート

Visual Studio 2026 で C++ の WinUI 3 プロジェクトを作成し、`.sln` のあるフォルダにこの `winui3_screen_capture_guide_cpp.md` を配置したら、以下の順番で指示を出してください。

### ステップ1（フェーズ1：アプリ内でのプレビュー表示）
> 「`winui3_screen_capture_guide_cpp.md` を参照して、まずは『フェーズ1: 他のアプリをキャプチャしてアプリ内に表示する』までを実装してください。コード内の `ScreenCaptureAppCpp` 名前空間は、現在のプロジェクトの実際の名前空間に合わせて書き換えてください。」

### ステップ2（フェーズ2：MP4録画と保存機能の追加）
> 「プレビュー表示が確認できました。続けて `winui3_screen_capture_guide_cpp.md` の『フェーズ2: キャプチャした映像を録画してMP4ファイルに保存する』を実装し、`MainWindow.xaml.cpp` のスタブ関数を置き換えて録画・MP4保存ができるようにしてください。」


---

## フェーズ 3: 実装時の修正ポイント・システム音声録音・クラス分割リファクタリング・画質設定メニューの追加

ここからは、フェーズ1・フェーズ2の実装後に追加・改善された機能および、実際の Visual Studio 2026 (C++/WinRT) 環境でのビルド時に判明した重要な修正ポイントをまとめます。

---

### 3.1 C++/WinRT ビルド時の重要な修正ポイント

1. **`Windows` 名前空間と `IInspectable` の曖昧さ回避**
   * `<windows.graphics.directx.direct3d11.interop.h>` をインクルードすると、グローバル名前空間に `::Windows`（および `<inspectable.h>` の `::IInspectable`）が定義されます。
   * そのため、`using namespace winrt;` と `using namespace Windows::Foundation;` を併用すると `error C2872: 'Windows': あいまいなシンボルです` や `'IInspectable': あいまいなシンボルです` が発生します。
   * **対策**: `using namespace winrt::Windows::Foundation;` のように必ず `winrt::` プレフィックスを明示し、`IInspectable` も `winrt::Windows::Foundation::IInspectable`（または COM 側は `::IInspectable`）と明記します。
2. **`PrepareTranscodeResult::TranscodeAsync()` の戻り値型**
   * `prepareOp.TranscodeAsync()` の戻り値は `IAsyncAction` ではなく **`winrt::Windows::Foundation::IAsyncActionWithProgress<double>`** です。メンバ変数 `m_transcodeOp` の型をこれに合わせる必要があります。
3. **`MainWindow.idl` の整理**
   * WinUI 3 プロジェクトテンプレート既定の `Int32 MyProperty;` が `MainWindow.idl` に残っていると、実装側で未実装エラーになるため削除します。
4. **ソースファイルの文字コード (UTF-8 BOM)**
   * 日本語コメントや日本語 UI 文字列を含む `.h` / `.cpp` ファイルは、MSVC のコードページ警告 (`C4819`) を防ぐため **UTF-8 (BOM付き)** で保存します。

---

### 3.2 システム音声（WASAPI ループバック）の同時録音

PC 上で再生されているシステム音声（デスクトップ音声）を動画と一緒に MP4 (H.264 + AAC) に録音する仕組みです。

* **使用 API**: WASAPI (`IMMDeviceEnumerator`, `IMMDevice`, `IAudioClient`, `IAudioCaptureClient`)
* **ループバック初期化**: 既定の再生デバイス (`eRender`, `eConsole`) に対して `AUDCLNT_STREAMFLAGS_LOOPBACK` フラグを指定して `IAudioClient::Initialize` を呼び出します。
* **無音時の自動パディング（重要）**:
  * Windows の WASAPI ループバックの仕様上、PC 上で何も音が鳴っていない時はパケットが送出されません。
  * そのままだと `MediaStreamSource::SampleRequested`（音声側）が待機状態のままブロックし、映像側のエンコードまで停止してしまいます。
  * これを防ぐため、音声キャプチャスレッド側で **20ms 以上パケットが来ない場合は自動的に 20ms 分の無音（ゼロ埋め）PCM データを生成してキューに供給**します。
* **`MediaStreamSource` への統合**:
  * `MediaStreamSource(videoDescriptor, audioDescriptor)` のように映像と音声の両方の Descriptor を渡して生成し、`SampleRequested` イベント内で `request.StreamDescriptor().try_as<AudioStreamDescriptor>()` を判定して音声バッファ (`CryptographicBuffer::CreateFromByteArray`) または映像テクスチャを振り分けて供給します。

---

### 3.3 3クラス構成へのリファクタリング（責務の分離）

`MainWindow.xaml.cpp` にすべての処理が集中するのを防ぐため、以下の3つの専門クラスと UI 制御クラスに分割しました。

| クラス名 | ファイル | 役割・責務 |
| :--- | :--- | :--- |
| **`Direct3DHelper`** | `Direct3DHelper.h`<br>`Direct3DHelper.cpp` | `ID3D11Device` / `ID3D11DeviceContext` / WinRT `IDirect3DDevice` の生成、マルチスレッド保護設定、`SwapChainPanel` 用の `IDXGISwapChain1` 生成・リサイズ・フレーム描画 (`Present1`)・解放 |
| **`ScreenCaptureSession`** | `ScreenCaptureSession.h`<br>`ScreenCaptureSession.cpp` | `GraphicsCaptureItem`、`Direct3D11CaptureFramePool`、`GraphicsCaptureSession` のライフサイクル管理、フレーム到着時のプレビュー描画と録画コールバック通知 |
| **`MediaRecorder`** | `MediaRecorder.h`<br>`MediaRecorder.cpp` | MP4 映像エンコード (`MediaStreamSource`, `MediaTranscoder`, D3D11 録画テクスチャ管理) および WASAPI ループバック音声キャプチャスレッドの管理 |
| **`MainWindow`** | `MainWindow.xaml`<br>`MainWindow.xaml.h`<br>`MainWindow.xaml.cpp` | UI イベント処理、キャプチャ対象ピッカー・保存先ファイルピッカーの表示、画質設定メニューの状態管理、上記3クラスの連携 |

---

### 3.4 メニューによる録画画質（解像度スケール・フレームレート）の設定機能

上部に `MenuBar`（「設定」メニュー）を追加し、録画時の画質とフレームレートを動的に切り替えられるようにしました。

* **画面サイズ (解像度)**:
  * **等倍 (`1/1`)** / **1/2 サイズ (`1/2`)** / **1/4 サイズ (`1/4`)** から選択可能。
  * `MediaStreamSource` の入力 `VideoStreamDescriptor` にはキャプチャ元の全体解像度（偶数補正済み）を渡し、出力側の `MediaEncodingProfile` の `Width` / `Height` に縮小後の解像度（偶数補正済み）を設定します。これにより `MediaTranscoder` の GPU スケーラーによって映像が切り取られることなく綺麗に縮小エンコードされます。
* **フレームレート**:
  * **60 fps** / **30 fps** / **20 fps** から選択可能。
  * `MediaEncodingProfile` の `FrameRate` と、フレーム待機タイムアウト時間 (`1000 / fps` ms) に反映されます。

---

## 最終版の完全なソースコード一覧（名前空間: `Capture_Gemini`）

### 1. `pch.h`

```cpp
#pragma once
#include <windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>

// Undefine GetCurrentTime macro to prevent
// conflict with Storyboard::GetCurrentTime
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <wil/cppwinrt_helpers.h>

// --- 画面キャプチャ・DirectX・録画・WASAPI録音用 追加ヘッダー ---
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.MediaProperties.h>
#include <winrt/Windows.Media.Transcoding.h>
#include <winrt/Windows.Security.Cryptography.h>

#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <shobjidl_core.h>
#include <microsoft.ui.xaml.window.h>
#include <microsoft.ui.xaml.media.dxinterop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <deque>
#include <vector>
#include <functional>
#include <memory>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "ole32.lib")
```

---

### 2. `Direct3DHelper.h` / `Direct3DHelper.cpp`

#### `Direct3DHelper.h`
```cpp
#pragma once

namespace Capture_Gemini
{
    class Direct3DHelper
    {
    public:
        Direct3DHelper();
        ~Direct3DHelper();

        void Initialize();

        void CreateSwapChainForPanel(
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel,
            winrt::Windows::Graphics::SizeInt32 size);

        void ResizeSwapChain(winrt::Windows::Graphics::SizeInt32 size);

        void PresentFrame(ID3D11Texture2D* frameTexture);

        void ReleaseSwapChain(
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel);

        ID3D11Device* Device() const noexcept { return m_d3dDevice.get(); }
        ID3D11DeviceContext* Context() const noexcept { return m_d3dContext.get(); }
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& WinRTDevice() const noexcept { return m_winrtD3dDevice; }

    private:
        winrt::com_ptr<ID3D11Device> m_d3dDevice;
        winrt::com_ptr<ID3D11DeviceContext> m_d3dContext;
        winrt::com_ptr<IDXGISwapChain1> m_swapChain;
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_winrtD3dDevice{ nullptr };
    };
}
```

#### `Direct3DHelper.cpp`
```cpp
#include "pch.h"
#include "Direct3DHelper.h"

using namespace winrt;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

namespace Capture_Gemini
{
    Direct3DHelper::Direct3DHelper() = default;

    Direct3DHelper::~Direct3DHelper() = default;

    void Direct3DHelper::Initialize()
    {
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0
        };

        winrt::check_hresult(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            creationFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            m_d3dDevice.put(),
            nullptr,
            m_d3dContext.put()
        ));

        // マルチスレッド保護を有効化 (FramePoolおよびMediaStreamSourceの別スレッドアクセス対策)
        if (auto multithread = m_d3dDevice.try_as<ID3D10Multithread>())
        {
            multithread->SetMultithreadProtected(TRUE);
        }

        // ID3D11Device から WinRT の IDirect3DDevice を生成
        auto dxgiDevice = m_d3dDevice.as<IDXGIDevice>();
        com_ptr<::IInspectable> inspectableDevice;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectableDevice.put()));
        m_winrtD3dDevice = inspectableDevice.as<IDirect3DDevice>();
    }

    void Direct3DHelper::CreateSwapChainForPanel(
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel,
        SizeInt32 size)
    {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = static_cast<UINT>(max(1, size.Width));
        desc.Height = static_cast<UINT>(max(1, size.Height));
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Stereo = FALSE;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

        auto dxgiDevice = m_d3dDevice.as<IDXGIDevice2>();
        com_ptr<IDXGIAdapter> dxgiAdapter;
        winrt::check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));
        com_ptr<IDXGIFactory2> dxgiFactory;
        winrt::check_hresult(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.put())));

        m_swapChain = nullptr;
        winrt::check_hresult(dxgiFactory->CreateSwapChainForComposition(
            m_d3dDevice.get(),
            &desc,
            nullptr,
            m_swapChain.put()
        ));

        auto panelNative = panel.as<::ISwapChainPanelNative>();
        winrt::check_hresult(panelNative->SetSwapChain(m_swapChain.get()));
    }

    void Direct3DHelper::ResizeSwapChain(SizeInt32 size)
    {
        if (!m_swapChain) return;

        m_swapChain->ResizeBuffers(
            2,
            static_cast<UINT>(max(1, size.Width)),
            static_cast<UINT>(max(1, size.Height)),
            DXGI_FORMAT_B8G8R8A8_UNORM,
            0
        );
    }

    void Direct3DHelper::PresentFrame(ID3D11Texture2D* frameTexture)
    {
        if (!m_swapChain || !frameTexture) return;

        com_ptr<ID3D11Texture2D> backBuffer;
        winrt::check_hresult(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.put())));

        D3D11_TEXTURE2D_DESC backDesc{};
        backBuffer->GetDesc(&backDesc);
        D3D11_TEXTURE2D_DESC frameDesc{};
        frameTexture->GetDesc(&frameDesc);

        D3D11_BOX sourceRegion{};
        sourceRegion.left = 0;
        sourceRegion.top = 0;
        sourceRegion.front = 0;
        sourceRegion.right = min(backDesc.Width, frameDesc.Width);
        sourceRegion.bottom = min(backDesc.Height, frameDesc.Height);
        sourceRegion.back = 1;

        m_d3dContext->CopySubresourceRegion(backBuffer.get(), 0, 0, 0, 0, frameTexture, 0, &sourceRegion);

        DXGI_PRESENT_PARAMETERS presentParams{};
        m_swapChain->Present1(1, 0, &presentParams);
    }

    void Direct3DHelper::ReleaseSwapChain(
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel)
    {
        if (m_swapChain)
        {
            if (panel)
            {
                auto panelNative = panel.as<::ISwapChainPanelNative>();
                panelNative->SetSwapChain(nullptr);
            }
            m_swapChain = nullptr;
        }
    }
}
```

---

### 3. `ScreenCaptureSession.h` / `ScreenCaptureSession.cpp`

#### `ScreenCaptureSession.h`
```cpp
#pragma once
#include "Direct3DHelper.h"

namespace Capture_Gemini
{
    class ScreenCaptureSession
    {
    public:
        using FrameCallback = std::function<void(ID3D11Texture2D*, winrt::Windows::Foundation::TimeSpan)>;
        using ClosedCallback = std::function<void()>;

        ScreenCaptureSession();
        ~ScreenCaptureSession();

        void StartCapture(
            Direct3DHelper& d3dHelper,
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item,
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& previewPanel,
            FrameCallback onFrameCallback,
            ClosedCallback onClosedCallback);

        void StopCapture(
            Direct3DHelper& d3dHelper,
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& previewPanel);

        bool IsCapturing() const noexcept { return m_captureItem != nullptr; }
        winrt::Windows::Graphics::SizeInt32 CurrentSize() const noexcept { return m_lastSize; }

    private:
        void OnFrameArrived(
            winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
            winrt::Windows::Foundation::IInspectable const& args);

        Direct3DHelper* m_d3dHelper{ nullptr };
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
        winrt::Windows::Graphics::SizeInt32 m_lastSize{ 0, 0 };
        winrt::event_token m_frameArrivedToken{};
        winrt::event_token m_closedToken{};

        FrameCallback m_onFrameCallback;
        ClosedCallback m_onClosedCallback;
    };
}
```

#### `ScreenCaptureSession.cpp`
```cpp
#include "pch.h"
#include "ScreenCaptureSession.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;

namespace Capture_Gemini
{
    ScreenCaptureSession::ScreenCaptureSession() = default;

    ScreenCaptureSession::~ScreenCaptureSession() = default;

    void ScreenCaptureSession::StartCapture(
        Direct3DHelper& d3dHelper,
        GraphicsCaptureItem const& item,
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& previewPanel,
        FrameCallback onFrameCallback,
        ClosedCallback onClosedCallback)
    {
        StopCapture(d3dHelper, previewPanel);

        m_d3dHelper = &d3dHelper;
        m_captureItem = item;
        m_lastSize = m_captureItem.Size();
        m_onFrameCallback = std::move(onFrameCallback);
        m_onClosedCallback = std::move(onClosedCallback);

        // 対象ウィンドウが閉じられたらコールバックへ通知
        m_closedToken = m_captureItem.Closed([this](auto&&, auto&&)
        {
            if (m_onClosedCallback)
            {
                m_onClosedCallback();
            }
        });

        // DXGI SwapChain の作成と SwapChainPanel へのセット
        m_d3dHelper->CreateSwapChainForPanel(previewPanel, m_lastSize);

        // FramePool の作成 (バックグラウンドスレッドで動作する CreateFreeThreaded を使用)
        m_framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(
            m_d3dHelper->WinRTDevice(),
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            m_lastSize
        );

        m_frameArrivedToken = m_framePool.FrameArrived({ this, &ScreenCaptureSession::OnFrameArrived });

        m_session = m_framePool.CreateCaptureSession(m_captureItem);
        m_session.StartCapture();
    }

    void ScreenCaptureSession::OnFrameArrived(
        Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const&)
    {
        auto frame = sender.TryGetNextFrame();
        if (!frame || !m_d3dHelper) return;

        auto contentSize = frame.ContentSize();
        if (contentSize.Width <= 0 || contentSize.Height <= 0) return;

        bool needRecreate = (contentSize.Width != m_lastSize.Width || contentSize.Height != m_lastSize.Height);
        if (needRecreate)
        {
            m_lastSize = contentSize;
            m_d3dHelper->ResizeSwapChain(m_lastSize);
        }

        // フレームの IDirect3DSurface からネイティブの ID3D11Texture2D を取得
        auto surface = frame.Surface();
        auto access = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        com_ptr<ID3D11Texture2D> frameTexture;
        winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(frameTexture.put())));

        // SwapChain に描画
        m_d3dHelper->PresentFrame(frameTexture.get());

        // 録画コールバックへフレームを渡す
        if (m_onFrameCallback)
        {
            m_onFrameCallback(frameTexture.get(), frame.SystemRelativeTime());
        }

        if (needRecreate)
        {
            m_framePool.Recreate(
                m_d3dHelper->WinRTDevice(),
                DirectXPixelFormat::B8G8R8A8UIntNormalized,
                2,
                m_lastSize
            );
        }
    }

    void ScreenCaptureSession::StopCapture(
        Direct3DHelper& d3dHelper,
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& previewPanel)
    {
        if (m_session)
        {
            m_session.Close();
            m_session = nullptr;
        }
        if (m_framePool)
        {
            m_framePool.FrameArrived(m_frameArrivedToken);
            m_framePool.Close();
            m_framePool = nullptr;
        }
        if (m_captureItem)
        {
            m_captureItem.Closed(m_closedToken);
            m_captureItem = nullptr;
        }

        d3dHelper.ReleaseSwapChain(previewPanel);
        m_d3dHelper = nullptr;
    }
}
```

---

### 4. `MediaRecorder.h` / `MediaRecorder.cpp`

#### `MediaRecorder.h`
```cpp
#pragma once
#include "Direct3DHelper.h"

namespace Capture_Gemini
{
    struct RecordingOptions
    {
        uint32_t scaleDivisor{ 1 }; // 1 = 等倍, 2 = 1/2, 4 = 1/4
        uint32_t frameRate{ 60 };   // 60, 30, 20 fps
        bool recordAudio{ true };
    };

    class MediaRecorder
    {
    public:
        MediaRecorder();
        ~MediaRecorder();

        winrt::Windows::Foundation::IAsyncOperation<bool> StartAsync(
            Direct3DHelper& d3dHelper,
            winrt::Windows::Storage::Streams::IRandomAccessStream outputStream,
            winrt::Windows::Graphics::SizeInt32 captureSize,
            RecordingOptions options);

        winrt::Windows::Foundation::IAsyncAction StopAsync();

        void ProcessFrame(ID3D11Texture2D* frameTexture, winrt::Windows::Foundation::TimeSpan systemRelativeTime);

        bool IsRecording() const noexcept { return m_isRecording.load(); }

    private:
        void OnMediaStreamSourceStarting(
            winrt::Windows::Media::Core::MediaStreamSource const& sender,
            winrt::Windows::Media::Core::MediaStreamSourceStartingEventArgs const& args);

        void OnMediaStreamSourceSampleRequested(
            winrt::Windows::Media::Core::MediaStreamSource const& sender,
            winrt::Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const& args);

        // WASAPI ループバック音声キャプチャ関連
        bool InitializeAudioCapture();
        void StartAudioCaptureThread();
        void StopAudioCapture();
        void AudioCaptureThreadProc();

        struct AudioChunk
        {
            std::vector<uint8_t> data;
            winrt::Windows::Foundation::TimeSpan timestamp;
            winrt::Windows::Foundation::TimeSpan duration;
        };

        Direct3DHelper* m_d3dHelper{ nullptr };
        std::atomic<bool> m_isRecording{ false };
        std::mutex m_recordingMutex;
        std::condition_variable m_frameReadyCv;
        bool m_hasNewFrame{ false };

        uint32_t m_sourceWidth{ 0 };
        uint32_t m_sourceHeight{ 0 };
        uint32_t m_encodeWidth{ 0 };
        uint32_t m_encodeHeight{ 0 };
        uint32_t m_frameRate{ 60 };

        winrt::com_ptr<ID3D11Texture2D> m_recordingTexture;
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface m_recordingSurface{ nullptr };
        winrt::Windows::Foundation::TimeSpan m_startTime{ 0 };
        winrt::Windows::Foundation::TimeSpan m_lastFrameTime{ 0 };

        winrt::Windows::Media::Core::MediaStreamSource m_mediaStreamSource{ nullptr };
        winrt::Windows::Media::Transcoding::MediaTranscoder m_transcoder{ nullptr };
        winrt::Windows::Storage::Streams::IRandomAccessStream m_outputStream{ nullptr };
        winrt::Windows::Foundation::IAsyncActionWithProgress<double> m_transcodeOp{ nullptr };
        winrt::event_token m_startingToken{};
        winrt::event_token m_sampleRequestedToken{};

        // 音声状態メンバ
        bool m_recordAudioEnabled{ true };
        winrt::com_ptr<IAudioClient> m_audioClient;
        winrt::com_ptr<IAudioCaptureClient> m_audioCaptureClient;
        uint32_t m_audioSampleRate{ 48000 };
        uint32_t m_audioChannels{ 2 };
        uint32_t m_audioBitsPerSample{ 32 };
        uint32_t m_audioBlockAlign{ 8 };
        bool m_audioIsFloat{ true };

        std::thread m_audioThread;
        std::atomic<bool> m_isAudioCapturing{ false };
        std::mutex m_audioMutex;
        std::condition_variable m_audioCv;
        std::deque<AudioChunk> m_audioQueue;
        uint64_t m_totalAudioFramesCaptured{ 0 };
    };
}
```

#### `MediaRecorder.cpp`
```cpp
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
```

---

### 5. `MainWindow.xaml` / `MainWindow.xaml.h` / `MainWindow.xaml.cpp`

#### `MainWindow.xaml`
```xml
<Window
    x:Class="Capture_Gemini.MainWindow"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
    xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
    mc:Ignorable="d"
    Title="WinUI 3 Screen Recorder (C++/WinRT)">

    <Grid RowDefinitions="Auto, Auto, *">
        <!-- メニューバー -->
        <MenuBar Grid.Row="0">
            <MenuBarItem Title="設定" x:Name="SettingsMenuBarItem">
                <MenuFlyoutSubItem Text="画面サイズ (解像度)">
                    <RadioMenuFlyoutItem x:Name="Scale1xMenuItem" Text="等倍 (1/1)" GroupName="ResolutionScale" IsChecked="True" Click="QualityMenuItem_Click" />
                    <RadioMenuFlyoutItem x:Name="ScaleHalfMenuItem" Text="1/2 サイズ" GroupName="ResolutionScale" Click="QualityMenuItem_Click" />
                    <RadioMenuFlyoutItem x:Name="ScaleQuarterMenuItem" Text="1/4 サイズ" GroupName="ResolutionScale" Click="QualityMenuItem_Click" />
                </MenuFlyoutSubItem>
                <MenuFlyoutSubItem Text="フレームレート">
                    <RadioMenuFlyoutItem x:Name="Fps60MenuItem" Text="60 fps" GroupName="FrameRate" IsChecked="True" Click="QualityMenuItem_Click" />
                    <RadioMenuFlyoutItem x:Name="Fps30MenuItem" Text="30 fps" GroupName="FrameRate" Click="QualityMenuItem_Click" />
                    <RadioMenuFlyoutItem x:Name="Fps20MenuItem" Text="20 fps" GroupName="FrameRate" Click="QualityMenuItem_Click" />
                </MenuFlyoutSubItem>
                <MenuFlyoutSeparator />
                <ToggleMenuFlyoutItem x:Name="RecordAudioMenuItem" Text="システム音声も録音する" IsChecked="True" Click="RecordAudioMenuItem_Click" />
            </MenuBarItem>
        </MenuBar>

        <!-- 操作パネル -->
        <StackPanel Grid.Row="1" Orientation="Horizontal" Spacing="12" Padding="16,10" Background="{ThemeResource SurfaceStrokeColorDefaultBrush}">
            <Button x:Name="StartCaptureButton" Content="キャプチャ対象を選択" Click="StartCaptureButton_Click" />
            <Button x:Name="StopCaptureButton" Content="キャプチャ停止" Click="StopCaptureButton_Click" IsEnabled="False" />
            
            <!-- フェーズ2で使用する録画ボタン -->
            <Button x:Name="StartRecordButton" Content="録画開始" Click="StartRecordButton_Click" IsEnabled="False" Background="Red" Foreground="White" />
            <Button x:Name="StopRecordButton" Content="録画停止・保存" Click="StopRecordButton_Click" IsEnabled="False" />

            <!-- システム音声録音チェックボックス -->
            <CheckBox x:Name="RecordAudioCheckBox" Content="システム音声も録音する" IsChecked="True" VerticalAlignment="Center" Margin="8,0,0,0" Click="RecordAudioCheckBox_Click" />

            <!-- 現在の録画設定ステータス表示 -->
            <TextBlock x:Name="QualityStatusTextBlock" Text="録画設定: 等倍 / 60 fps" VerticalAlignment="Center" Margin="12,0,0,0" Opacity="0.8" />
        </StackPanel>

        <!-- プレビュー表示領域 -->
        <Border Grid.Row="2" Margin="16" BorderBrush="Gray" BorderThickness="1">
            <SwapChainPanel x:Name="PreviewSwapChainPanel" />
        </Border>
    </Grid>
</Window>
```

#### `MainWindow.xaml.h`
```cpp
#pragma once

#include "MainWindow.g.h"
#include "Direct3DHelper.h"
#include "ScreenCaptureSession.h"
#include "MediaRecorder.h"

namespace winrt::Capture_Gemini::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

        void StartCaptureButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void StopCaptureButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void StartRecordButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void StopRecordButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void QualityMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void RecordAudioMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void RecordAudioCheckBox_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

    private:
        HWND GetWindowHandle();
        void UpdateQualitySettingsFromUI();
        winrt::Windows::Foundation::IAsyncAction StartCaptureAsync();
        void StopCapture();
        winrt::Windows::Foundation::IAsyncAction StartRecordAsync();
        winrt::Windows::Foundation::IAsyncAction StopRecordAsync();

        ::Capture_Gemini::Direct3DHelper m_d3dHelper;
        ::Capture_Gemini::ScreenCaptureSession m_captureSession;
        ::Capture_Gemini::MediaRecorder m_mediaRecorder;
        ::Capture_Gemini::RecordingOptions m_recordingOptions;
    };
}

namespace winrt::Capture_Gemini::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
```

#### `MainWindow.xaml.cpp`
```cpp
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
```