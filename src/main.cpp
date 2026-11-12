#include <memory>
#include "imgui.h"
#include "app/utils.h"
#include "app/win32_dx11_app.h"
#include "ts_parser.h"

constexpr size_t PacketContentColumnSize = 6;
constexpr uint8_t kBom[] = { 0xEF, 0xBB, 0xBF };

bool app_running = true;

struct MainCtx
{
    struct Gui
    {
        enum class FeatureTypes
        {
            kInfo,
            kPacket,
        };

        struct Packet
        {
            struct Combo
            {
                std::string show_str;
                std::string save_str;
                int pid = 0;
            };

            std::vector<std::shared_ptr<Combo>> combos;
            size_t combo_index = 0;

            std::vector<std::string> content_column_descriptions;
            std::vector<std::vector<std::string>> content_strs;
            int content_selected_row = 0;
            int content_selected_column = 0;
        };

        FeatureTypes feature_type = FeatureTypes::kInfo;
        Packet packet;
    };

    std::shared_ptr<app::Application> app;
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

static void ShowInfo(MainCtx *h)
{
    if (!h->ts_parser)
    {
        return;
    }

    size_t width = 15;

    app::ImGuiTextFmt("{:<{}}", "文件大小：", width);
    ImGui::SameLine();
    app::ImGuiTextFmt("{}", h->ts_parser->GetFileSize());

    app::ImGuiTextFmt("{:<{}}", "ts包长度：", width);
    ImGui::SameLine();
    app::ImGuiTextFmt("{}", h->ts_parser->GetTsPacketSize());
}

static void PrepareContent(MainCtx *h, int combo_index)
{
    MainCtx::Gui::Packet *packet = &h->gui.packet;
    packet->content_strs.clear();

    std::vector<TsParser::TsPacket *> *content_packets = nullptr;
    if (0 == combo_index)
    {
        content_packets = h->ts_parser->GetPidPacket(-2);
    }

    if ((content_packets) && (!content_packets->empty()))
    {
        for (size_t index = 0; index < content_packets->size(); ++index)
        {
            TsParser::TsPacket *ts_packet = content_packets->at(index);

            std::vector<std::string> row(PacketContentColumnSize);
            row[0] = fmt::format("{:<10d}", ts_packet->index);
            row[1] = fmt::format("{:<#05x} {:5d}", ts_packet->header.pid, ts_packet->header.pid);
            packet->content_strs.push_back(row);
        }
    }

    packet->combo_index = combo_index;
    packet->content_selected_row = 0;
}

static void PacketContentSaveToCsv(MainCtx *h)
{
    MainCtx::Gui::Packet *packet = &h->gui.packet;
    std::string text = fmt::format("{}_{}", h->file_name, packet->combos[packet->combo_index]->save_str);
    std::wstring file_name_w = h->app->SaveFileDialog(app::Utf8ToUtf16(text));

    FILE *file = _wfopen(file_name_w.c_str(), L"wb");
    if (file)
    {
        // bom
        fwrite(kBom, 1, sizeof(kBom), file);

        // 第一行描述行
        {
            std::string line;
            for (auto &&str : packet->content_column_descriptions)
            {
                if (line.empty())
                {
                    line = fmt::format("{}", str);
                }
                else
                {
                    line = fmt::format("{}, {}", line, str);
                }
            }
            line += "\n";
            fwrite(line.c_str(), 1, line.size(), file);
        }

        for (size_t row = 0; row < packet->content_strs.size(); ++row)
        {
            std::string line;
            for (auto &&str : packet->content_strs[row])
            {
                if (line.empty())
                {
                    line = fmt::format("{}", str);
                }
                else
                {
                    line = fmt::format("{}, {}", line, str);
                }
            }
            line += "\n";
            fwrite(line.c_str(), 1, line.size(), file);
        }
        fclose(file);
    }
}

static void ShowPacket(MainCtx *h)
{
    MainCtx::Gui::Packet *packet = &h->gui.packet;

    if (!h->ts_parser)
    {
        return;
    }

    if (packet->combos.empty())
    {
        {
            std::shared_ptr<MainCtx::Gui::Packet::Combo> combo = std::make_shared<MainCtx::Gui::Packet::Combo>();
            combo->show_str = "所有数据包";
            combo->save_str = fmt::format("all");
            packet->combos.push_back(combo);
        }

        for (auto &&pid : h->ts_parser->GetPids())
        {
            std::shared_ptr<MainCtx::Gui::Packet::Combo> combo = std::make_shared<MainCtx::Gui::Packet::Combo>();
            combo->show_str = fmt::format("pid=0x{:x}({:d})", pid, pid);
            combo->save_str = fmt::format("pid_{}", pid);
            combo->pid = pid;
            packet->combos.push_back(combo);
        }

        packet->content_column_descriptions.clear();
        packet->content_column_descriptions.push_back("序号");
        packet->content_column_descriptions.push_back("pid");
        packet->content_column_descriptions.push_back("类型");
        packet->content_column_descriptions.push_back("pcr");
        packet->content_column_descriptions.push_back("dts");
        packet->content_column_descriptions.push_back("pts");
        PrepareContent(h, 0);
    }

    if (ImGui::BeginCombo("##列表分类", packet->combos[packet->combo_index]->show_str.c_str()))
    {
        for (size_t combo_index = 0; combo_index < packet->combos.size(); ++combo_index)
        {
            bool selected = (packet->combo_index == combo_index);
            if (ImGui::Selectable(packet->combos[combo_index]->show_str.c_str(), selected))
            {
                if (packet->combo_index != combo_index)
                {
                    PrepareContent(h, combo_index);
                }
            }
        }

        ImGui::EndCombo();
    }

    if (!packet->content_strs.empty())
    {
        bool open_content_menu = false;

        if (ImGui::BeginTable("ContentTable", PacketContentColumnSize,
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingFixedFit |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY,
            ImVec2(0, 0)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            for (size_t i = 0; i < PacketContentColumnSize; ++i)
            {
                ImGui::TableSetupColumn(packet->content_column_descriptions[i].c_str());
            }
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(packet->content_strs.size()));

            while (clipper.Step())
            {
                for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; index++)
                {
                    std::vector<std::string> &row_strs = packet->content_strs[index];

                    ImGui::PushID(index);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);

                    // 左键选中
                    if (ImGui::Selectable(
                        row_strs[0].c_str(),
                        packet->content_selected_row == index,
                        ImGuiSelectableFlags_SpanAllColumns))
                    {
                        packet->content_selected_row = index;
                    }

                    // 右键也选中当前行
                    if (ImGui::IsItemHovered() &&
                        ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    {
                        packet->content_selected_row = index;

                        int col = ImGui::TableGetHoveredColumn();
                        packet->content_selected_column = col;
                        open_content_menu = true;
                    }

                    ImGui::TableSetColumnIndex(1);
                    app::ImGuiTextFmt("{}", row_strs[1]);

                    ImGui::PopID();
                }
            }

            ImGui::EndTable();
        }

        // 统一管理右键菜单
        if (open_content_menu)
        {
            ImGui::OpenPopup("process_content_menu");
        }

        if (ImGui::BeginPopup("process_content_menu"))
        {
            if ((packet->content_selected_row < static_cast<int>(packet->content_strs.size())) &&
                (packet->content_selected_column < PacketContentColumnSize))
            {
                std::string text = fmt::format("复制值：{}", packet->content_strs[packet->content_selected_row][packet->content_selected_column]);
                if (ImGui::MenuItem(text.c_str()))
                {
                    ImGui::SetClipboardText(packet->content_strs[packet->content_selected_row][packet->content_selected_column].c_str());
                }
            }

            if (ImGui::MenuItem("导出为csv..."))
            {
                PacketContentSaveToCsv(h);
            }

            ImGui::EndPopup();
        }
    }
}

int main(int argc, char **argv)
{
    std::shared_ptr<MainCtx> h = std::make_shared<MainCtx>();

    h->app = std::make_shared<app::Application>();
    if (!h->app->Init(L"Ts Parser", 800, 600))
    {
        return 1;
    }

    while (h->app->ProcessMessages())
    {
        h->app->BeginFrame();

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
            h->file_path_w = h->app->OpenFileDialog();
            h->file_name = GetFileName(app::Utf16ToUtf8(h->file_path_w));

            h->ts_parser = std::make_shared<TsParser>();
            if (!h->ts_parser->Init(h->file_path_w))
            {
                h->ts_parser = nullptr;
            }
        }

        ImGui::SameLine();

        app::ImGuiTextFmt("{}", h->file_name);

        // 左侧导航菜单
        ImGui::BeginChild("Sidebar", ImVec2(180, 0), true);

        if (ImGui::Selectable("文件信息", MainCtx::Gui::FeatureTypes::kInfo == h->gui.feature_type))
        {
            h->gui.feature_type = MainCtx::Gui::FeatureTypes::kInfo;
        }

        if (ImGui::Selectable("包信息", MainCtx::Gui::FeatureTypes::kPacket == h->gui.feature_type))
        {
            h->gui.feature_type = MainCtx::Gui::FeatureTypes::kPacket;
        }

        ImGui::EndChild();

        ImGui::SameLine();

        // 右侧子页面
        ImGui::BeginChild("Content", ImVec2(0, 0), true);

        switch (h->gui.feature_type)
        {
        case MainCtx::Gui::FeatureTypes::kInfo:
            ShowInfo(h.get());
            break;

        case MainCtx::Gui::FeatureTypes::kPacket:
            ShowPacket(h.get());
            break;

        default:
            break;
        }

        ImGui::EndChild();

        // ImGui窗口定义结束
        ImGui::End();

        h->app->EndFrame();
    }

    h->app = nullptr;
    h = nullptr;
    return 0;
}
