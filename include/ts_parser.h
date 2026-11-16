#ifndef TS_PARSER_INCLUDE_TS_PARSER_H_
#define TS_PARSER_INCLUDE_TS_PARSER_H_

#include <windows.h>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

constexpr int64_t kTsMod = 1LL << 33;
constexpr int64_t kPcrMod = (1LL << 33) * 300;

class TsParser
{
public:
    struct TsPacket
    {
        enum TypeFlags : int
        {
            kError = 1 << 0,
            kPad = 1 << 1,
            kPat = 1 << 5,
            kPmt = 1 << 6,
            kSdt = 1 << 7,
        };

        struct TsHeader
        {
            int sync_byte = 0;
            int transport_error_indicator = 0;
            int payload_unit_start_indicator = 0;
            int transport_priority = 0;
            int pid = -1;
            int transport_scrambling_control = 0;
            int adaptation_field_control = 0;
            int continuity_counter = 0;
        };

        struct AdaptationField
        {
            const uint8_t *data = nullptr;
            size_t size = 0;

            int adaptation_field_length = 0;
            int64_t pcr = kPcrMod;
        };

        struct Payload
        {
            struct Psi
            {
                const uint8_t *data = nullptr;
                size_t size = 0;

                int table_id = 0;
                int section_length = 0;
            };

            struct Pes
            {
                enum StreamIds : int
                {
                    kProgramStreamMap = 0xbc,
                    kPrivateStream1 = 0xbd,
                    kPaddingStream = 0xbe,
                    kPrivateStream2 = 0xbf,
                    kAudioStream0 = 0xc0,
                    kVideoStream0 = 0xe0,
                    kEcmStream = 0xf0,
                    kEmmStream = 0xf1,
                    kDsmccStream = 0xf2,
                    kTypeEStream = 0xf8,
                    kMetadataStream = 0xfc,
                    kExtenedStreamId = 0xfd,
                    kProgramStreamDirectory = 0xff
                };

                const uint8_t *data = nullptr;
                size_t size = 0;

                int stream_id = 0;
                int pes_packet_length = 0;
                int64_t pts = kTsMod;
                int64_t dts = kTsMod;
            };

            struct Rest
            {
                const uint8_t *data = nullptr;
                size_t size = 0;
            };

            const uint8_t *data = nullptr;
            size_t size = 0;

            std::shared_ptr<Psi> psi;
            std::shared_ptr<Pes> pes;
            std::shared_ptr<Rest> rest;
        };

        struct Undefined
        {
            const uint8_t *data = nullptr;
            size_t size = 0;
        };

        size_t packet_index = 0;
        const uint8_t *data = nullptr;
        size_t size = 0;

        int type_flags = 0;

        std::shared_ptr<TsHeader> ts_header;
        std::shared_ptr<AdaptationField> adaptation_field;
        std::shared_ptr<Payload> payload;
        std::shared_ptr<Undefined> undef;
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
    bool ParseAdaptionField(TsPacket *ts_packet, const uint8_t *&packet_ptr);
    bool ParsePes(TsPacket *ts_packet, const uint8_t *&packet_ptr);
    bool ParsePsi(TsPacket *ts_packet, const uint8_t *&packet_ptr);

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
