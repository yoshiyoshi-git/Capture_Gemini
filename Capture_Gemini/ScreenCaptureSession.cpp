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
