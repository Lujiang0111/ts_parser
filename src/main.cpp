#include <memory>
#include "imgui.h"
#include "win32_dx11_app.h"

bool app_running = true;

int main(int argc, char **argv)
{
    std::shared_ptr<Application> app = std::make_shared<Application>();

    if (!app->Init(L"My ImGui Tool", 1280, 800))
    {
        return 1;
    }

    while (app->ProcessMessages())
    {
        app->BeginFrame();

        // 获取 Win32 窗口对应的 ImGui 主视口
        ImGuiViewport *viewport = ImGui::GetMainViewport();

        // 让 ImGui 窗口覆盖整个客户区
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        // 以后开发具体工具，主要修改这里和自己的业务模块。
        ImGui::Begin("MainWindow", nullptr, flags);

        ImGui::Text("Win32 + DirectX 11 + Dear ImGui");

        static int counter = 0;
        if (ImGui::Button("点击我"))
        {
            ++counter;
        }

        ImGui::SameLine();
        ImGui::Text("Counter: %d", counter);

        ImGui::End();

        app->EndFrame();
    }

    app = nullptr;
    return 0;
}
