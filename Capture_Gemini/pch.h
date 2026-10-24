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
