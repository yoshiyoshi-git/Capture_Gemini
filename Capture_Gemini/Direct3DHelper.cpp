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
