#pragma once

namespace Capture_Gemini
{
    // Direct3D 11 デバイスとプレビュー用 SwapChain を管理するクラス
    class Direct3DHelper
    {
    public:
        Direct3DHelper();
        ~Direct3DHelper();

        // D3D11 デバイス・コンテキスト・WinRT デバイスを生成・初期化する
        // 引数: なし / 戻り値: なし
        void Initialize();

        // SwapChain を新規作成し、指定の SwapChainPanel に紐付ける
        // 引数: panel = 描画先パネル, size = 初期サイズ(幅・高さ) / 戻り値: なし
        void CreateSwapChainForPanel(
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel,
            winrt::Windows::Graphics::SizeInt32 size);

        // SwapChain のバッファサイズを変更する
        // 引数: size = 新しいサイズ(幅・高さ) / 戻り値: なし
        void ResizeSwapChain(winrt::Windows::Graphics::SizeInt32 size);

        // キャプチャしたテクスチャを SwapChain にコピーして画面表示する
        // 引数: frameTexture = 描画元テクスチャ / 戻り値: なし
        void PresentFrame(ID3D11Texture2D* frameTexture);

        // SwapChainPanel との紐付けを解除し、SwapChain を破棄する
        // 引数: panel = 解除対象のパネル / 戻り値: なし
        void ReleaseSwapChain(
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel);

        // ネイティブ D3D11 デバイスを取得する (引数: なし / 戻り値: ID3D11Device ポインタ)
        ID3D11Device* Device() const noexcept { return m_d3dDevice.get(); }

        // ネイティブ D3D11 コンテキストを取得する (引数: なし / 戻り値: ID3D11DeviceContext ポインタ)
        ID3D11DeviceContext* Context() const noexcept { return m_d3dContext.get(); }

        // WinRT 用の Direct3D デバイスを取得する (引数: なし / 戻り値: IDirect3DDevice 参照)
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& WinRTDevice() const noexcept { return m_winrtD3dDevice; }

    private:
        winrt::com_ptr<ID3D11Device> m_d3dDevice;                                               // D3D11 デバイス
        winrt::com_ptr<ID3D11DeviceContext> m_d3dContext;                                       // D3D11 コンテキスト
        winrt::com_ptr<IDXGISwapChain1> m_swapChain;                                            // プレビュー用 SwapChain
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_winrtD3dDevice{ nullptr }; // WinRT 連携用デバイス
    };
}
