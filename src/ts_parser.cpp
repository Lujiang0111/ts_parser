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

    if (!ParseTsFile())
    {
        return false;
    }

    for (auto &&ts_packet : ts_packets_)
    {
        pids_.insert(ts_packet->header.pid);

        if (pid_packet_map_.end() == pid_packet_map_.find(-2))
        {
            std::shared_ptr<std::vector<TsPacket *>> val = std::make_shared<std::vector<TsPacket *>>();
            pid_packet_map_.emplace(-2, val);
        }
        pid_packet_map_[-2]->push_back(ts_packet.get());

        if (pid_packet_map_.end() == pid_packet_map_.find(ts_packet->header.pid))
        {
            std::shared_ptr<std::vector<TsPacket *>> val = std::make_shared<std::vector<TsPacket *>>();
            pid_packet_map_.emplace(ts_packet->header.pid, val);
        }
        pid_packet_map_[ts_packet->header.pid]->push_back(ts_packet.get());
    }

    return true;
}

void TsParser::Deinit()
{
    pid_packet_map_.clear();
    pids_.clear();
    ts_packets_.clear();

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

size_t TsParser::GetFileSize() const
{
    return file_size_;
}

size_t TsParser::GetTsPacketSize() const
{
    return ts_packet_size_;
}

const std::set<int> &TsParser::GetPids() const
{
    return pids_;
}

std::vector<TsParser::TsPacket *> *TsParser::GetPidPacket(int pid)
{
    auto iter = pid_packet_map_.find(pid);
    if (pid_packet_map_.end() == iter)
    {
        return nullptr;
    }
    return iter->second.get();
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

bool TsParser::ParseTsFile()
{
    ts_packet_size_ = 188;
    const uint8_t *file_end = file_data_ + file_size_;
    const uint8_t *error_start_ptr = nullptr;
    const uint8_t *ptr = file_data_;
    while (file_end != ptr)
    {
        if (IsTsHeader(ptr, file_end, ts_packet_size_))
        {
            if (error_start_ptr)
            {
                ParseTsPacket(error_start_ptr, ptr - error_start_ptr);
                error_start_ptr = nullptr;
            }

            ParseTsPacket(ptr, ts_packet_size_);
            ptr += ts_packet_size_;
        }
        else
        {
            error_start_ptr = ptr;
            ++ptr;
        }
    }

    if (error_start_ptr)
    {
        ParseTsPacket(error_start_ptr, file_end - error_start_ptr);
        error_start_ptr = nullptr;
    }

    return true;
}

constexpr size_t kTsHeaderSize = 4;

void TsParser::ParseTsPacket(const uint8_t *data, size_t size)
{
    std::shared_ptr<TsParser::TsPacket> ts_packet = std::make_shared<TsParser::TsPacket>();
    ts_packets_.push_back(ts_packet);

    ts_packet->index = ts_packets_.size() - 1;
    ts_packet->data = data;
    ts_packet->size = size;

    const uint8_t *end = ts_packet->data + ts_packet->size;
    const uint8_t *ptr = ts_packet->data;

    if (ptr + kTsHeaderSize >= end)
    {
        ts_packet->type = TsPacket::Types::kError;
        return;
    }

    TsPacket::TsHeader *ts_header = &ts_packet->header;
    
    ts_header->sync_byte = ptr[0];
    if (0x47 != ts_header->sync_byte)
    {
        ts_packet->type = TsPacket::Types::kError;
        return;
    }

    ts_header->transport_error_indicator = ((ptr[1] >> 7) & 0x01);
    ts_header->payload_unit_start_indicator = ((ptr[1] >> 6) & 0x01);
    ts_header->transport_priority = ((ptr[1] >> 5) & 0x01);
    ts_header->pid = ((ptr[1] & 0x1f) << 8) + ptr[2];
    ts_header->transport_scrambling_control = ((ptr[3] >> 6) & 0x03);
    ts_header->adaption_field_control = ((ptr[3] >> 4) & 0x03);
    ts_header->continuity_counter = (ptr[3] & 0x0f);

    ptr += kTsHeaderSize;
}
