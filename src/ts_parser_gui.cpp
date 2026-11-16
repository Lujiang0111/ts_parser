#include "app/utils.h"
#include "ts_parser_gui.h"

TsParserGui::TsParserGui()
{

}

TsParserGui::~TsParserGui()
{
    Deinit();
}

bool TsParserGui::Init()
{
    app_ = std::make_shared<app::Application>();
    if (!app_->Init(L"Ts Parser", 800, 600))
    {
        return false;
    }

    gui_ = std::make_shared<Gui>();
    return true;
}

void TsParserGui::Deinit()
{
    ts_parser_ = nullptr;
    gui_ = nullptr;
    app_ = nullptr;
}

static std::string GetFileName(const std::string &file_path)
{
    size_t pos = file_path.find_last_of("/\\");
    if (std::string::npos == pos)
    {
        return file_path;
    }

    return file_path.substr(pos + 1);
}

void TsParserGui::Do()
{
    while (app_->ProcessMessages())
    {
        app_->BeginFrame();

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
            file_path_w_ = app_->OpenFileDialog();
            file_name_ = GetFileName(app::Utf16ToUtf8(file_path_w_));

            ts_parser_ = std::make_shared<TsParser>();
            if (!ts_parser_->Init(file_path_w_))
            {
                ts_parser_ = nullptr;
            }
        }

        ImGui::SameLine();

        app::ImGuiTextFmt("{}", file_name_);

        // 左侧导航菜单
        ImGui::BeginChild("Sidebar", ImVec2(180, 0), true);

        if (ImGui::Selectable("文件信息", Gui::Types::kMainInfo == gui_->type))
        {
            gui_->type = Gui::Types::kMainInfo;
        }

        if (ImGui::Selectable("包信息", Gui::Types::kPacketInfo == gui_->type))
        {
            gui_->type = Gui::Types::kPacketInfo;
        }

        ImGui::EndChild();

        ImGui::SameLine();

        // 右侧子页面
        ImGui::BeginChild("Content", ImVec2(0, 0), true);

        switch (gui_->type)
        {
        case Gui::Types::kMainInfo:
            ShowMainInfo();
            break;

        case Gui::Types::kPacketInfo:
            ShowPacketInfo();
            break;

        default:
            break;
        }

        ImGui::EndChild();

        // ImGui窗口定义结束
        ImGui::End();

        app_->EndFrame();
    }
}
