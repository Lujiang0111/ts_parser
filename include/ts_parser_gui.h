#ifndef TS_PARSER_INCLUDE_TS_PARSER_GUI_H_
#define TS_PARSER_INCLUDE_TS_PARSER_GUI_H_

#include "app/win32_dx11_app.h"
#include "ts_parser.h"

class TsParserGui
{
public:
    TsParserGui(const TsParserGui &) = delete;
    TsParserGui &operator=(const TsParserGui &) = delete;

    TsParserGui();
    virtual ~TsParserGui();

    bool Init();
    void Deinit();

    void Do();

private:
    struct Gui
    {
        enum class Types
        {
            kMainInfo,
            kPacketInfo,
        };

        struct PacketInfo
        {
            struct Filter
            {
                std::string show_str;
                std::string save_str;
                int pid = 0;
            };

            struct SubFilter
            {
                enum class Types
                {
                    kAll = 0,
                    kWorthy,
                };

                std::string show_str;
                std::string save_str;
                Types type = Types::kAll;
            };

            std::vector<std::shared_ptr<Filter>> filters;
            size_t filter_index = 0;

            std::vector<std::shared_ptr<SubFilter>> sub_filters;
            size_t sub_filter_index = 0;

            std::vector<TsParser::TsPacket *> filtered_packets;

            std::vector<std::string> table_column_descriptions;
            size_t table_selected_row = 0;
            size_t table_selected_column = 0;
        };

        Types type = Types::kMainInfo;
        std::shared_ptr<PacketInfo> packet_info;
    };

private:
    void ShowMainInfo();

    void ShowPacketInfo();
    void PreparePacketInfoTable(size_t filter_index, size_t sub_filter_index);
    std::shared_ptr<std::vector<std::string>> GetPacketInfoTableRowStr(size_t row_index);
    void ExportPacketInfoTableToCsv();

private:
    std::shared_ptr<app::Application> app_;
    std::shared_ptr<Gui> gui_;

    std::wstring file_path_w_;
    std::string file_name_;
    std::shared_ptr<TsParser> ts_parser_;
};

#endif // !TS_PARSER_INCLUDE_TS_PARSER_GUI_H_
