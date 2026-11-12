#ifndef IMGUI_APP_WIN32_DX11_APP_H_
#define IMGUI_APP_WIN32_DX11_APP_H_

#include <string>
#include <windows.h>
#include <d3d11.h>

namespace app
{

class Application
{
public:
    Application();
    virtual ~Application();

    bool Init(const wchar_t *title, int width = 1280, int height = 800);
    void Deinit();

    // 处理 Windows 消息；返回 false 表示窗口已关闭。
    bool ProcessMessages();

    // 每帧开始前调用一次。
    void BeginFrame();

    // 每帧 UI 绘制完成后调用一次。
    void EndFrame();

    //打开文件封装
    std::wstring OpenFileDialog();

    std::wstring SaveFileDialog(const std::wstring &default_name = L"data");

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

    bool CreateDevice();
    bool CreateRenderTarget();
    void DestroyRenderTarget();
    void DestroyDevice();

private:
    HWND hwnd_;
    HINSTANCE hinstance_;

    bool running_;
    bool minimized_;
    bool class_registered_;

    bool imgui_win32_inited_;
    bool imgui_dx11_inited_;

    ID3D11Device *device_;
    ID3D11DeviceContext *context_;
    IDXGISwapChain *swap_chain_;
    ID3D11RenderTargetView *render_target_;
};

}

#endif // !IMGUI_APP_WIN32_DX11_APP_H_
