#include <memory>
#include "imgui.h"
#include "lccl/oss/fmt.h"
#include "ts_parser.h"
#include "win32_dx11_app.h"

bool app_running = true;

struct MainCtx
{
    struct Gui
    {
        enum class PageTypes
        {
            kInfo,
            kPacket,
        };

        PageTypes page_type = PageTypes::kInfo;
    };

    Gui gui;

    std::wstring file_path_w;
    std::string file_name;

    std::shared_ptr<TsParser> ts_parser;
};

static std::string GetFileName(const std::string &file_path)
{
    size_t pos = file_path.find_last_of("/\\");
    if (std::string::npos == pos)
    {
        return file_path;
    }

    return file_path.substr(pos + 1);
}

int main(int argc, char **argv)
{
    std::shared_ptr<MainCtx> ctx = std::make_shared<MainCtx>();

    std::shared_ptr<Application> app = std::make_shared<Application>();
    if (!app->Init(L"Ts Parser", 800, 600))
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

        // ImGui窗口定义
        ImGui::Begin("Ts parser", nullptr, flags);

        if (ImGui::Button("打开文件"))
        {
            ctx->file_path_w = app->OpenFileDialog();
            ctx->file_name = GetFileName(app->Utf16ToUtf8(ctx->file_path_w));

            ctx->ts_parser = std::make_shared<TsParser>();
            if (!ctx->ts_parser->Init(ctx->file_path_w))
            {
                ctx->ts_parser = nullptr;
            }
        }

        ImGui::SameLine();

        ImGui::Text("%s", ctx->file_name.c_str());

        // 左侧导航菜单
        ImGui::BeginChild("Sidebar", ImVec2(180, 0), true);

        if (ImGui::Selectable("文件信息", MainCtx::Gui::PageTypes::kInfo == ctx->gui.page_type))
        {
            ctx->gui.page_type = MainCtx::Gui::PageTypes::kInfo;
        }

        if (ImGui::Selectable("包信息", MainCtx::Gui::PageTypes::kPacket == ctx->gui.page_type))
        {
            ctx->gui.page_type = MainCtx::Gui::PageTypes::kPacket;
        }

        ImGui::EndChild();

        ImGui::SameLine();

        // 右侧子页面
        ImGui::BeginChild("Content", ImVec2(0, 0), true);

        switch (ctx->gui.page_type)
        {
        case MainCtx::Gui::PageTypes::kInfo:
            ImGui::Text("显示设置页面");
            break;

        case MainCtx::Gui::PageTypes::kPacket:
            ImGui::Text("声音设置页面");
            break;

        default:
            break;
        }

        ImGui::EndChild();

        // ImGui窗口定义结束
        ImGui::End();

        app->EndFrame();
    }

    app = nullptr;
    return 0;
}
