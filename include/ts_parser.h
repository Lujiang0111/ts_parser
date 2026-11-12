#ifndef TS_PARSER_INCLUDE_TS_PARSER_H_
#define TS_PARSER_INCLUDE_TS_PARSER_H_

#include <windows.h>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class TsParser
{
public:
    struct TsPacket
    {
        enum class Types
        {
            kError = 0,
            kOther,
        };

        struct TsHeader
        {
            int sync_byte = 0;
            int transport_error_indicator = 0;
            int payload_unit_start_indicator = 0;
            int transport_priority = 0;
            int pid = -1;
            int transport_scrambling_control = 0;
            int adaption_field_control = 0;
            int continuity_counter = 0;
        };

        size_t index = 0;
        const uint8_t *data = nullptr;
        size_t size = 0;

        Types type = Types::kError;
        TsHeader header;
    };

public:
    TsParser(const TsParser &) = delete;
    TsParser &operator=(const TsParser &) = delete;

    TsParser();
    virtual ~TsParser();

    bool Init(const std::wstring &file_path_w);
    void Deinit();

    size_t GetFileSize() const;
    size_t GetTsPacketSize() const;
    const std::set<int> &GetPids() const;
    std::vector<TsPacket *> *GetPidPacket(int pid);

private:
    bool OpenFile(const std::wstring &file_path_w);
    bool ParseTsFile();
    void ParseTsPacket(const uint8_t *data, size_t size);

private:
    std::wstring file_name_;

    HANDLE file_;
    HANDLE file_mapping_;
    void *file_mapped_;

    const uint8_t *file_data_;
    size_t file_size_;

    size_t ts_packet_size_;
    std::vector<std::shared_ptr<TsPacket>> ts_packets_;

    /**
    * key=-2代表所有包
    * key=-1代表所有异常包
    * 其他代表对应pid的包
    */
    std::unordered_map<int, std::shared_ptr<std::vector<TsPacket *>>> pid_packet_map_;

    std::set<int> pids_;
};

#endif // !TS_PARSER_INCLUDE_TS_PARSER_H_
