#include "app/utils.h"
#include "ts_parser_gui.h"

void TsParserGui::ShowMainInfo()
{
    if (!ts_parser_)
    {
        return;
    }

    size_t width = 15;
    app::ImGuiTextFmt("{:<{}}", "文件路径：", width);
    ImGui::SameLine();
    app::ImGuiTextFmt("{}", app::Utf16ToUtf8(file_path_w_));

    app::ImGuiTextFmt("{:<{}}", "文件大小：", width);
    ImGui::SameLine();
    app::ImGuiTextFmt("{}", ts_parser_->GetFileSize());

    app::ImGuiTextFmt("{:<{}}", "ts包长度：", width);
    ImGui::SameLine();
    app::ImGuiTextFmt("{}", ts_parser_->GetTsPacketSize());
}
