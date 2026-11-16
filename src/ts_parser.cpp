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
        pids_.insert(ts_packet->ts_header->pid);

        if (pid_packet_map_.end() == pid_packet_map_.find(-2))
        {
            std::shared_ptr<std::vector<TsPacket *>> val = std::make_shared<std::vector<TsPacket *>>();
            pid_packet_map_.emplace(-2, val);
        }
        pid_packet_map_[-2]->push_back(ts_packet.get());

        if (ts_packet->type_flags & TsPacket::TypeFlags::kError)
        {
            if (pid_packet_map_.end() == pid_packet_map_.find(-1))
            {
                std::shared_ptr<std::vector<TsPacket *>> val = std::make_shared<std::vector<TsPacket *>>();
                pid_packet_map_.emplace(-1, val);
            }
            pid_packet_map_[-1]->push_back(ts_packet.get());
        }

        if (pid_packet_map_.end() == pid_packet_map_.find(ts_packet->ts_header->pid))
        {
            std::shared_ptr<std::vector<TsPacket *>> val = std::make_shared<std::vector<TsPacket *>>();
            pid_packet_map_.emplace(ts_packet->ts_header->pid, val);
        }
        pid_packet_map_[ts_packet->ts_header->pid]->push_back(ts_packet.get());
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

void TsParser::GetPidPacket(int pid, std::deque<TsPacket *> &ts_packets)
{
    ts_packets.clear();

    auto iter = pid_packet_map_.find(pid);
    if (pid_packet_map_.end() == iter)
    {
        return;
    }

    for (auto &&ts_packet : *iter->second)
    {
        ts_packets.push_back(ts_packet);
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
    std::shared_ptr<TsPacket> ts_packet = std::make_shared<TsPacket>();
    ts_packets_.push_back(ts_packet);

    ts_packet->packet_index = ts_packets_.size() - 1;
    ts_packet->data = data;
    ts_packet->size = size;

    const uint8_t *end = ts_packet->data + ts_packet->size;
    const uint8_t *ptr = ts_packet->data;

    if (ptr + kTsHeaderSize > end)
    {
        ts_packet->type_flags |= TsPacket::TypeFlags::kError;
        return;
    }

    ts_packet->ts_header = std::make_shared<TsPacket::TsHeader>();
    TsPacket::TsHeader *ts_header = ts_packet->ts_header.get();

    ts_header->sync_byte = ptr[0];
    if (0x47 != ts_header->sync_byte)
    {
        ts_packet->type_flags |= TsPacket::TypeFlags::kError;
        return;
    }

    ts_header->transport_error_indicator = ((ptr[1] >> 7) & 0x01);
    ts_header->payload_unit_start_indicator = ((ptr[1] >> 6) & 0x01);
    ts_header->transport_priority = ((ptr[1] >> 5) & 0x01);
    ts_header->pid = ((ptr[1] & 0x1f) << 8) + ptr[2];
    if (0x1ff == ts_header->pid)
    {
        ts_packet->type_flags |= TsPacket::TypeFlags::kPad;
    }

    ts_header->transport_scrambling_control = ((ptr[3] >> 6) & 0x03);
    ts_header->adaptation_field_control = ((ptr[3] >> 4) & 0x03);
    if (0 == ts_header->adaptation_field_control)
    {
        ts_packet->type_flags |= TsPacket::TypeFlags::kError;
    }

    ts_header->continuity_counter = (ptr[3] & 0x0f);
    ptr += kTsHeaderSize;

    if (ts_header->adaptation_field_control & 0b10)
    {
        if (!ParseAdaptionField(ts_packet.get(), ptr))
        {
            return;
        }
    }

    if (ts_header->adaptation_field_control & 0b01)
    {
        ts_packet->payload = std::make_shared<TsPacket::Payload>();
        ts_packet->payload->data = ptr;
        ts_packet->payload->size = end - ptr;

        if (ts_header->payload_unit_start_indicator)
        {
            if (ptr + 3 > end)
            {
                ts_packet->type_flags |= TsPacket::TypeFlags::kError;
                return;
            }

            if ((0x00 == ptr[0]) && (0x00 == ptr[1]) && (0x01 == ptr[2]))
            {
                if (!ParsePes(ts_packet.get(), ptr))
                {
                    return;
                }
            }
            else
            {
                if (!ParsePsi(ts_packet.get(), ptr))
                {
                    return;
                }
            }
        }

        if (ptr < end)
        {
            ts_packet->payload->rest = std::make_shared<TsPacket::Payload::Rest>();
            ts_packet->payload->rest->data = ptr;
            ts_packet->payload->rest->size = end - ptr;
            ptr = ts_packet->payload->rest->data + ts_packet->payload->rest->size;
        }
    }

    if (ptr < end)
    {
        ts_packet->undef = std::make_shared<TsPacket::Undefined>();
        ts_packet->undef->data = ptr;
        ts_packet->undef->size = end - ptr;
        ptr = ts_packet->undef->data + ts_packet->undef->size;
    }
}

bool TsParser::ParseAdaptionField(TsPacket *ts_packet, const uint8_t *&packet_ptr)
{
    const uint8_t *ptr = packet_ptr;
    const uint8_t *end = ts_packet->data + ts_packet->size;

    ts_packet->adaptation_field = std::make_shared<TsPacket::AdaptationField>();
    TsPacket::AdaptationField *adaptation_field = ts_packet->adaptation_field.get();
    adaptation_field->data = ptr;

    adaptation_field->adaptation_field_length = ptr[0];
    adaptation_field->size = 1 + adaptation_field->adaptation_field_length;
    if (adaptation_field->data + adaptation_field->size > end)
    {
        ts_packet->type_flags |= TsPacket::TypeFlags::kError;
        return false;
    }

    ++ptr;
    if (adaptation_field->adaptation_field_length >= 7)
    {
        int pcr_flag = ((ptr[0] >> 4) & 0x01);
        if (pcr_flag)
        {
            ++ptr;
            adaptation_field->pcr = (static_cast<int64_t>(ptr[0]) << 34) +
                (static_cast<int64_t>(ptr[1]) << 26) +
                (static_cast<int64_t>(ptr[2]) << 18) +
                (static_cast<int64_t>(ptr[3]) << 10) +
                (static_cast<int64_t>((ptr[4] >> 7) & 0x01) << 9) +
                (static_cast<int64_t>(ptr[4] & 0x01) << 8) +
                static_cast<int64_t>(ptr[5]);
        }
    }

    packet_ptr = adaptation_field->data + adaptation_field->size;
    return true;
}

bool TsParser::ParsePes(TsPacket *ts_packet, const uint8_t *&packet_ptr)
{
    const uint8_t *ptr = packet_ptr;
    const uint8_t *end = ts_packet->data + ts_packet->size;

    ts_packet->payload->pes = std::make_shared<TsPacket::Payload::Pes>();
    TsPacket::Payload::Pes *pes = ts_packet->payload->pes.get();
    pes->data = ptr;

    if ((ptr + 3 > end) || (0x00 != ptr[0]) || (0x00 != ptr[1]) || (0x01 != ptr[2]))
    {
        ts_packet->type_flags |= TsPacket::TypeFlags::kError;
        return false;
    }
    ptr += 3;

    if (ptr + 3 > end)
    {
        ts_packet->type_flags |= TsPacket::TypeFlags::kError;
        return false;
    }
    pes->stream_id = ptr[0];
    pes->pes_packet_length = (ptr[1] << 8) + ptr[2];
    ptr += 3;

    if ((TsPacket::Payload::Pes::StreamIds::kProgramStreamMap != pes->stream_id) &&
        (TsPacket::Payload::Pes::StreamIds::kPaddingStream != pes->stream_id) &&
        (TsPacket::Payload::Pes::StreamIds::kPrivateStream2 != pes->stream_id) &&
        (TsPacket::Payload::Pes::StreamIds::kEcmStream != pes->stream_id) &&
        (TsPacket::Payload::Pes::StreamIds::kEmmStream != pes->stream_id) &&
        (TsPacket::Payload::Pes::StreamIds::kProgramStreamDirectory != pes->stream_id) &&
        (TsPacket::Payload::Pes::StreamIds::kDsmccStream != pes->stream_id) &&
        (TsPacket::Payload::Pes::StreamIds::kTypeEStream != pes->stream_id))
    {
        if (ptr + 3 > end)
        {
            ts_packet->type_flags |= TsPacket::TypeFlags::kError;
            return false;
        }
        int pts_dts_flags = ((ptr[1] >> 6) & 0x03);
        int pes_header_data_length = ptr[2];
        ptr += 3;

        if (ptr + pes_header_data_length > end)
        {
            ts_packet->type_flags |= TsPacket::TypeFlags::kError;
            return false;
        }
        pes->size = ptr + pes_header_data_length - pes->data;

        if (pts_dts_flags & 0b10)
        {
            if (ptr + 5 > end)
            {
                ts_packet->type_flags |= TsPacket::TypeFlags::kError;
                return false;
            }

            pes->pts = (static_cast<int64_t>((ptr[0] >> 1) & 0x07) << 30) +
                (static_cast<int64_t>(ptr[1]) << 22) +
                (static_cast<int64_t>((ptr[2] >> 1) & 0x7f) << 15) +
                (static_cast<int64_t>(ptr[3]) << 7) +
                static_cast<int64_t>((ptr[4] >> 1) & 0x7f);
            ptr += 5;
        }

        if (pts_dts_flags & 0b01)
        {
            if (ptr + 5 > end)
            {
                ts_packet->type_flags |= TsPacket::TypeFlags::kError;
                return false;
            }

            pes->dts = (static_cast<int64_t>((ptr[0] >> 1) & 0x07) << 30) +
                (static_cast<int64_t>(ptr[1]) << 22) +
                (static_cast<int64_t>((ptr[2] >> 1) & 0x7f) << 15) +
                (static_cast<int64_t>(ptr[3]) << 7) +
                static_cast<int64_t>((ptr[4] >> 1) & 0x7f);
            ptr += 5;
        }
    }
    else
    {
        pes->size = ptr - pes->data;
    }

    packet_ptr = pes->data + pes->size;
    return true;
}

bool TsParser::ParsePsi(TsPacket *ts_packet, const uint8_t *&packet_ptr)
{
    const uint8_t *ptr = packet_ptr;
    const uint8_t *end = ts_packet->data + ts_packet->size;

    if (ptr + 1 > end)
    {
        ts_packet->type_flags |= TsPacket::TypeFlags::kError;
        return false;
    }
    int pointer_field = ptr[0];
    ++ptr;
    
    if (ptr + pointer_field > end)
    {
        ts_packet->type_flags |= TsPacket::TypeFlags::kError;
        return false;
    }
    ptr += pointer_field;

    ts_packet->payload->psi = std::make_shared<TsPacket::Payload::Psi>();
    TsPacket::Payload::Psi *psi = ts_packet->payload->psi.get();
    psi->data = ptr;

    if (ptr + 12 > end)
    {
        ts_packet->type_flags |= TsPacket::TypeFlags::kError;
        return false;
    }

    psi->table_id = ptr[0];
    ++ptr;
    switch (psi->table_id)
    {
    case 0x00:
        ts_packet->type_flags |= TsPacket::TypeFlags::kPat;
        break;
    case 0x02:
        ts_packet->type_flags |= TsPacket::TypeFlags::kPmt;
        break;
    case 0x46:
        ts_packet->type_flags |= TsPacket::TypeFlags::kSdt;
        break;
    default:
        break;
    }

    psi->section_length = ((ptr[0] & 0x0f) << 8) + ptr[1];
    ptr += 2;

    if (ptr + psi->section_length > end)
    {
        psi->size = end - psi->data;
    }
    else
    {
        psi->size = ptr + psi->section_length - psi->data;
    }

    packet_ptr = psi->data + psi->size;
    return true;
}
