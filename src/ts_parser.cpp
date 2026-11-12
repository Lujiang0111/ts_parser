#include "ts_parser.h"

TsParser::TsParser() :
    file_(nullptr),
    file_mapping_(nullptr),
    file_mapped_(nullptr),
    file_data_(nullptr),
    file_size_(0),
    ts_packet_size_(188)
{

}

TsParser::~TsParser()
{
    Deinit();
}

bool TsParser::Init(const std::wstring &file_path_w)
{
    if (!OpenFile(file_path_w))
    {
        return false;
    }

    if (!ParseTs())
    {
        return false;
    }

    return true;
}

void TsParser::Deinit()
{
    if (file_mapped_)
    {
        UnmapViewOfFile(file_mapped_);
        file_mapped_ = nullptr;
    }

    if (file_mapping_)
    {
        CloseHandle(file_mapping_);
        file_mapping_ = nullptr;
    }

    if (file_)
    {
        CloseHandle(file_);
        file_ = nullptr;
    }
}

bool TsParser::OpenFile(const std::wstring &file_path_w)
{
    // 打开文件
    file_ = CreateFileW(
        file_path_w.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (INVALID_HANDLE_VALUE == file_)
    {
        file_ = nullptr;
        return false;
    }

    // 获取文件大小
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_, &file_size))
    {
        return false;
    }

    if (0 == file_size.QuadPart)
    {
        return false;
    }


    // 创建文件映射对象
    file_mapping_ = CreateFileMappingW(
        file_,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr
    );

    if (!file_mapping_)
    {
        return false;
    }

    // 映射整个文件
    file_mapped_ = MapViewOfFile(
        file_mapping_,
        FILE_MAP_READ,
        0,
        0,
        0
    );

    if (!file_mapped_)
    {
        return false;
    }

    file_data_ = static_cast<const uint8_t *>(file_mapped_);
    file_size_ = static_cast<size_t>(file_size.QuadPart);
    return true;
}

static bool IsTsHeader(const uint8_t *ptr, const uint8_t *end, size_t &ts_packet_size)
{
    if (0x47 != ptr[0])
    {
        return false;
    }

    if ((ptr + ts_packet_size == end) ||
        ((ptr + ts_packet_size < end) && (0x47 == ptr[ts_packet_size])))
    {
        return true;
    }

    constexpr size_t kTsPacketSizes[] = { 188, 192, 204 };
    for (auto &&predict_ts_packet_size : kTsPacketSizes)
    {
        if ((ptr + predict_ts_packet_size < end) && (0x47 == ptr[predict_ts_packet_size]))
        {
            ts_packet_size = predict_ts_packet_size;
            return true;
        }
    }

    return false;
}

bool TsParser::ParseTs()
{
    ts_packet_size_ = 188;
    const uint8_t *file_end = file_data_ + file_size_;
    const uint8_t *ptr = file_data_;
    while (file_end != ptr)
    {
        if (IsTsHeader(ptr, file_end, ts_packet_size_))
        {

            ptr += ts_packet_size_;
        }
        else
        {
            ++ptr;
        }
    }

    return true;
}
