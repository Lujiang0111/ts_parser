#include "app/utils.h"
#include "ts_parser_gui.h"

constexpr size_t kTableColumnSize = 7;
constexpr uint8_t kBom[] = { 0xEF, 0xBB, 0xBF };

void TsParserGui::ShowPacketInfo()
{
    if (!gui_->packet_info)
    {
        gui_->packet_info = std::make_shared<Gui::PacketInfo>();
    }
    Gui::PacketInfo *packet_info = gui_->packet_info.get();

    if (packet_info->filters.empty())
    {
        {
            std::shared_ptr<Gui::PacketInfo::Filter> filter = std::make_shared<Gui::PacketInfo::Filter>();
            filter->show_str = "所有数据包";
            filter->save_str = fmt::format("all");
            filter->pid = -2;
            packet_info->filters.push_back(filter);
        }

        {
            std::shared_ptr<Gui::PacketInfo::Filter> filter = std::make_shared<Gui::PacketInfo::Filter>();
            filter->show_str = "异常包";
            filter->save_str = fmt::format("error");
            filter->pid = -1;
            packet_info->filters.push_back(filter);
        }

        for (auto &&pid : ts_parser_->GetPids())
        {
            if (pid >= 0)
            {
                std::shared_ptr<Gui::PacketInfo::Filter> filter = std::make_shared<Gui::PacketInfo::Filter>();
                filter->show_str = fmt::format("pid=0x{:x}({:d})", pid, pid);
                filter->save_str = fmt::format("pid_{}", pid);
                filter->pid = pid;
                packet_info->filters.push_back(filter);
            }
        }

        {
            std::shared_ptr<Gui::PacketInfo::SubFilter> sub_filter = std::make_shared<Gui::PacketInfo::SubFilter>();
            sub_filter->show_str = "显示所有";
            sub_filter->save_str = fmt::format("");
            sub_filter->type = Gui::PacketInfo::SubFilter::Types::kAll;
            packet_info->sub_filters.push_back(sub_filter);
        }

        {
            std::shared_ptr<Gui::PacketInfo::SubFilter> sub_filter = std::make_shared<Gui::PacketInfo::SubFilter>();
            sub_filter->show_str = "显示可解析";
            sub_filter->save_str = fmt::format("_worthy");
            sub_filter->type = Gui::PacketInfo::SubFilter::Types::kWorthy;
            packet_info->sub_filters.push_back(sub_filter);
        }

        PreparePacketInfoTable(0, 0);
    }

    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##过滤分类", packet_info->filters[packet_info->filter_index]->show_str.c_str()))
    {
        for (size_t filter_index = 0; filter_index < packet_info->filters.size(); ++filter_index)
        {
            bool selected = (packet_info->filter_index == filter_index);
            if (ImGui::Selectable(packet_info->filters[filter_index]->show_str.c_str(), selected))
            {
                if (packet_info->filter_index != filter_index)
                {
                    PreparePacketInfoTable(filter_index, 0);
                }
            }
        }

        ImGui::EndCombo();
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##显示分类", packet_info->sub_filters[packet_info->sub_filter_index]->show_str.c_str()))
    {
        for (size_t sub_filter_index = 0; sub_filter_index < packet_info->sub_filters.size(); ++sub_filter_index)
        {
            bool selected = (packet_info->sub_filter_index == sub_filter_index);
            if (ImGui::Selectable(packet_info->sub_filters[sub_filter_index]->show_str.c_str(), selected))
            {
                if (packet_info->sub_filter_index != sub_filter_index)
                {
                    PreparePacketInfoTable(packet_info->filter_index, sub_filter_index);
                }
            }
        }

        ImGui::EndCombo();
    }

    if (!packet_info->filtered_packets.empty())
    {
        bool open_context_menu = false;

        if (ImGui::BeginTable("ContentTable", kTableColumnSize,
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingFixedFit |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY,
            ImVec2(0, 0)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            for (size_t i = 0; i < kTableColumnSize; ++i)
            {
                ImGui::TableSetupColumn(packet_info->table_column_descriptions[i].c_str());
            }
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(packet_info->filtered_packets.size()));

            while (clipper.Step())
            {
                for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
                {
                    std::shared_ptr<std::vector<std::string>> row_strs = GetPacketInfoTableRowStr(index);

                    ImGui::PushID(index);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);

                    // 左键选中
                    if (ImGui::Selectable(
                        row_strs->at(0).c_str(),
                        packet_info->table_selected_row == index,
                        ImGuiSelectableFlags_SpanAllColumns))
                    {
                        packet_info->table_selected_row = index;
                    }

                    // 右键也选中当前行
                    if (ImGui::IsItemHovered() &&
                        ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    {
                        packet_info->table_selected_row = index;

                        int col = ImGui::TableGetHoveredColumn();
                        packet_info->table_selected_column = col;
                        open_context_menu = true;
                    }

                    for (int column_index = 1; column_index < kTableColumnSize; ++column_index)
                    {
                        ImGui::TableSetColumnIndex(column_index);
                        app::ImGuiTextFmt("{}", row_strs->at(column_index));
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndTable();
        }

        // 统一管理右键菜单
        if (open_context_menu)
        {
            ImGui::OpenPopup("process_content_menu");
        }

        if (ImGui::BeginPopup("process_content_menu"))
        {
            if (packet_info->table_selected_row < packet_info->filtered_packets.size())
            {
                if (packet_info->table_selected_column < kTableColumnSize)
                {
                    std::shared_ptr<std::vector<std::string>> row_strs = GetPacketInfoTableRowStr(packet_info->table_selected_row);
                    std::string text = fmt::format("复制值：{}", row_strs->at(packet_info->table_selected_column));
                    if (ImGui::MenuItem(text.c_str()))
                    {
                        ImGui::SetClipboardText(row_strs->at(packet_info->table_selected_column).c_str());
                    }
                }
            }

            if (ImGui::MenuItem("导出为csv..."))
            {
                ExportPacketInfoTableToCsv();
            }

            ImGui::EndPopup();
        }
    }
}

void TsParserGui::PreparePacketInfoTable(size_t filter_index, size_t sub_filter_index)
{
    Gui::PacketInfo *packet_info = gui_->packet_info.get();

    packet_info->filter_index = filter_index;
    packet_info->sub_filter_index = sub_filter_index;

    Gui::PacketInfo::Filter *filter = packet_info->filters[packet_info->filter_index].get();
    ts_parser_->GetPidPacket(filter->pid, packet_info->filtered_packets);

    switch (packet_info->sub_filters[packet_info->sub_filter_index]->type)
    {
    case Gui::PacketInfo::SubFilter::Types::kWorthy:
    {
        auto iter = packet_info->filtered_packets.begin();
        while (packet_info->filtered_packets.end() != iter)
        {
            TsParser::TsPacket *ts_packet = *iter;
            if ((ts_packet->type_flags & TsParser::TsPacket::TypeFlags::kError) ||
                (ts_packet->type_flags & TsParser::TsPacket::TypeFlags::kPad) ||
                (ts_packet->type_flags & TsParser::TsPacket::TypeFlags::kPat) ||
                (ts_packet->type_flags & TsParser::TsPacket::TypeFlags::kPmt) ||
                (ts_packet->type_flags & TsParser::TsPacket::TypeFlags::kSdt) ||
                ((ts_packet->adaptation_field) && (kPcrMod != ts_packet->adaptation_field->pcr)) ||
                ((ts_packet->payload) && (ts_packet->payload->pes) && (kTsMod != ts_packet->payload->pes->dts)) ||
                ((ts_packet->payload) && (ts_packet->payload->pes) && (kTsMod != ts_packet->payload->pes->pts)) ||
                ((ts_packet->payload) && (ts_packet->payload->psi)))
            {
                ++iter;
            }
            else
            {
                iter = packet_info->filtered_packets.erase(iter);
            }
        }

        break;
    }

    default:
        break;
    }

    packet_info->table_column_descriptions.resize(kTableColumnSize);
    packet_info->table_column_descriptions[0] = fmt::format("{:<10}", "序号");
    packet_info->table_column_descriptions[1] = fmt::format("{:<13}", "偏移");
    packet_info->table_column_descriptions[2] = fmt::format("{:<11}", "pid");
    packet_info->table_column_descriptions[3] = fmt::format("{:<6}", "类型");
    packet_info->table_column_descriptions[4] = fmt::format("{:<25}", "pcr");
    packet_info->table_column_descriptions[5] = fmt::format("{:<10}", "dts");
    packet_info->table_column_descriptions[6] = fmt::format("{:<10}", "pts");

    packet_info->table_selected_row = 0;
    packet_info->table_selected_column = 0;
}

std::shared_ptr<std::vector<std::string>> TsParserGui::GetPacketInfoTableRowStr(size_t row_index)
{
    Gui::PacketInfo *packet_info = gui_->packet_info.get();
    TsParser::TsPacket *first_ts_packet = packet_info->filtered_packets[0];
    TsParser::TsPacket *ts_packet = packet_info->filtered_packets[row_index];

    std::shared_ptr<std::vector<std::string>> row_strs = std::make_shared<std::vector<std::string>>(kTableColumnSize);
    row_strs->at(0) = fmt::format("{}", ts_packet->packet_index);
    row_strs->at(1) = fmt::format("{:#x}", ts_packet->data - first_ts_packet->data);
    row_strs->at(2) = fmt::format("{0:<#5x} {0:<5d}", ts_packet->ts_header->pid);
    
    if (ts_packet->type_flags & TsParser::TsPacket::TypeFlags::kError)
    {
        row_strs->at(3) = fmt::format("{}", "error");
    }
    else if(ts_packet->type_flags & TsParser::TsPacket::TypeFlags::kPad)
    {
        row_strs->at(3) = fmt::format("{}", "pad");
    }
    else if (ts_packet->type_flags & TsParser::TsPacket::TypeFlags::kPat)
    {
        row_strs->at(3) = fmt::format("{}", "pat");
    }
    else if (ts_packet->type_flags & TsParser::TsPacket::TypeFlags::kPmt)
    {
        row_strs->at(3) = fmt::format("{}", "pmt");
    }
    else if (ts_packet->type_flags & TsParser::TsPacket::TypeFlags::kSdt)
    {
        row_strs->at(3) = fmt::format("{}", "sdt");
    }
    else
    {
        if (ts_packet->adaptation_field)
        {
            if (kPcrMod != ts_packet->adaptation_field->pcr)
            {
                row_strs->at(3) = fmt::format("{}", "pcr");
                row_strs->at(4) = fmt::format("{}", ts_packet->adaptation_field->pcr);
            }
            else
            {
                row_strs->at(3) = fmt::format("{}", "adapt");
            }
        }

        if (ts_packet->payload)
        {
            if (ts_packet->payload->pes)
            {
                row_strs->at(3) = fmt::format("{}", "pes");
                if (kTsMod != ts_packet->payload->pes->dts)
                {
                    row_strs->at(5) = fmt::format("{}", ts_packet->payload->pes->dts);
                }
                if (kTsMod != ts_packet->payload->pes->pts)
                {
                    row_strs->at(6) = fmt::format("{}", ts_packet->payload->pes->pts);
                }
            }
            else if (ts_packet->payload->psi)
            {
                row_strs->at(3) = fmt::format("{}", "psi");
            }
            else
            {
                row_strs->at(3) = fmt::format("{}", "data");
            }
        }
        
        if (row_strs->at(3).empty())
        {
            row_strs->at(3) = fmt::format("{}", "other");
        }
    }

    return row_strs;
}

void TsParserGui::ExportPacketInfoTableToCsv()
{
    Gui::PacketInfo *packet_info = gui_->packet_info.get();

    std::string file_name = fmt::format("{}_{}{}",
        file_name_,
        packet_info->filters[packet_info->filter_index]->save_str,
        packet_info->sub_filters[packet_info->sub_filter_index]->save_str);
    std::wstring file_name_w = app_->SaveFileDialog(app::Utf8ToUtf16(file_name));

    FILE *file = _wfopen(file_name_w.c_str(), L"wb");
    if (file)
    {
        // bom
        fwrite(kBom, 1, sizeof(kBom), file);

        // 第一行描述行
        {
            std::string line;
            for (auto &&str : packet_info->table_column_descriptions)
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

        for (size_t index = 0; index < packet_info->filtered_packets.size(); ++index)
        {
            std::shared_ptr<std::vector<std::string>> row_strs = GetPacketInfoTableRowStr(index);

            std::string line;
            for (auto &&str : *row_strs)
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
