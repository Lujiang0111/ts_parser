#ifndef TS_PARSER_INCLUDE_TS_PARSER_H_
#define TS_PARSER_INCLUDE_TS_PARSER_H_

#include <string>
#include <windows.h>

class TsParser
{
public:
    TsParser(const TsParser &) = delete;
    TsParser &operator=(const TsParser &) = delete;

    TsParser();
    virtual ~TsParser();

    bool Init(const std::wstring &file_path_w);
    void Deinit();

private:
    bool OpenFile(const std::wstring &file_path_w);
    bool ParseTs();

private:
    std::wstring file_name_;

    HANDLE file_;
    HANDLE file_mapping_;
    void *file_mapped_;

    const uint8_t *file_data_;
    size_t file_size_;

    size_t ts_packet_size_;
};

#endif // !TS_PARSER_INCLUDE_TS_PARSER_H_
