#include <vector>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "win32_dx11_app.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

constexpr wchar_t *kWindowClassName = L"SimpleImGuiDx11Window";

static std::string Utf16ToUtf8(const std::wstring &utf16)
{
    if (utf16.empty())
    {
        return "";
    }

    // 计算 UTF-8 所需字节数
    int size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        utf16.data(),
        static_cast<int>(utf16.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (size <= 0)
    {
        return "";
    }

    // 分配空间
    std::vector<char> utf8(size + 1, 0);

    // 执行转换
    if (0 == WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        utf16.data(),
        static_cast<int>(utf16.size()),
        utf8.data(),
        size,
        nullptr,
        nullptr))
    {
        return "";
    }

    return utf8.data();
}

Application::Application()
    : hwnd_(nullptr),
    hinstance_(nullptr),
    running_(false),
    minimized_(false),
    class_registered_(false),
    imgui_win32_inited_(false),
    imgui_dx11_inited_(false),
    device_(nullptr),
    context_(nullptr),
    swap_chain_(nullptr),
    render_target_(nullptr)
{
}

Application::~Application()
{
    Deinit();
}

bool Application::Init(const wchar_t *title, int width, int height)
{
    hinstance_ = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = Application::WindowProc;
    wc.hInstance = hinstance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&wc))
    {
        return false;
    }
    class_registered_ = true;

    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowExW(
        0,
        kWindowClassName,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hinstance_,
        this
    );

    if (!hwnd_)
    {
        return false;
    }

    if (!CreateDevice())
    {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    ImFont *font = io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/msyh.ttc",
        18.0f
    );

    // 设置深色主题
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(hwnd_))
    {
        return false;
    }
    imgui_win32_inited_ = true;

    if (!ImGui_ImplDX11_Init(device_, context_))
    {
        return false;
    }
    imgui_dx11_inited_ = true;

    running_ = true;
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

void Application::Deinit()
{
    running_ = false;

    if (imgui_dx11_inited_)
    {
        ImGui_ImplDX11_Shutdown();
        imgui_dx11_inited_ = false;
    }

    if (imgui_win32_inited_)
    {
        ImGui_ImplWin32_Shutdown();
        imgui_win32_inited_ = false;
    }

    ImGui::DestroyContext();
    DestroyDevice();

    if ((hwnd_) && (IsWindow(hwnd_)))
    {
        DestroyWindow(hwnd_);
    }
    hwnd_ = nullptr;

    if (class_registered_)
    {
        UnregisterClassW(kWindowClassName, hinstance_);
        class_registered_ = false;
    }
}

bool Application::ProcessMessages()
{
    MSG msg;

    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            running_ = false;
            break;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return running_;
}

void Application::BeginFrame()
{
    if ((!imgui_dx11_inited_) || (!imgui_win32_inited_))
    {
        return;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Application::EndFrame()
{
    ImGui::Render();

    if ((minimized_) || (!context_) || (!render_target_))
    {
        return;
    }

    constexpr float kClearColors[] = { 0.10f, 0.10f, 0.12f, 1.00f };
    context_->OMSetRenderTargets(1, &render_target_, nullptr);
    context_->ClearRenderTargetView(render_target_, kClearColors);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    swap_chain_->Present(1, 0);
}

std::string Application::OpenFileDialog()
{
    wchar_t filename[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"所有文件\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn))
    {
        return Utf16ToUtf8(filename);
    }

    return "";
}

bool Application::CreateDevice()
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd_;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL kFeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        kFeatureLevels,
        ARRAYSIZE(kFeatureLevels),
        D3D11_SDK_VERSION,
        &sd,
        &swap_chain_,
        &device_,
        &feature_level,
        &context_
    );

    if (FAILED(hr))
    {
        return false;
    }

    return CreateRenderTarget();
}

bool Application::CreateRenderTarget()
{
    if ((!swap_chain_) || (!device_))
    {
        return false;
    }

    ID3D11Texture2D *back_buffer = nullptr;

    HRESULT hr = swap_chain_->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void **>(&back_buffer)
    );

    if (FAILED(hr))
    {
        return false;
    }

    hr = device_->CreateRenderTargetView(
        back_buffer,
        nullptr,
        &render_target_
    );

    back_buffer->Release();

    return SUCCEEDED(hr);
}

void Application::DestroyRenderTarget()
{
    if (render_target_)
    {
        render_target_->Release();
        render_target_ = nullptr;
    }
}

void Application::DestroyDevice()
{
    DestroyRenderTarget();

    if (context_)
    {
        context_->ClearState();
        context_->Flush();
    }

    if (swap_chain_)
    {
        swap_chain_->Release();
        swap_chain_ = nullptr;
    }

    if (context_)
    {
        context_->Release();
        context_ = nullptr;
    }

    if (device_)
    {
        device_->Release();
        device_ = nullptr;
    }
}

LRESULT CALLBACK Application::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    Application *app = reinterpret_cast<Application *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW *createStruct = reinterpret_cast<CREATESTRUCTW *>(l_param);

        app = static_cast<Application *>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->hwnd_ = hwnd;
    }

    if (app)
    {
        return app->HandleMessage(hwnd, message, w_param, l_param);
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT Application::HandleMessage(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    if ((imgui_win32_inited_) && (ImGui::GetCurrentContext()))
    {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, message, w_param, l_param))
        {
            return 1;
        }
    }

    switch (message)
    {
    case WM_SIZE:
        minimized_ = (w_param == SIZE_MINIMIZED);

        if ((!minimized_) && (swap_chain_) && (context_))
        {
            const UINT new_width = LOWORD(l_param);
            const UINT new_height = HIWORD(l_param);
            if ((new_width > 0) && (new_height > 0))
            {
                context_->OMSetRenderTargets(0, nullptr, nullptr);
                DestroyRenderTarget();

                HRESULT hr = swap_chain_->ResizeBuffers(
                    0,
                    new_width,
                    new_height,
                    DXGI_FORMAT_UNKNOWN,
                    0
                );

                if (SUCCEEDED(hr))
                {
                    CreateRenderTarget();
                }
            }
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((w_param & 0xFFF0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        running_ = false;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}
