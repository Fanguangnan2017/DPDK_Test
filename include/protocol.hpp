#pragma once

#include <cstddef>
#include <cstdint>

namespace industrial {

constexpr uint16_t kChannelCount = 12;
constexpr uint16_t kChannelsPerPort = 6;
constexpr uint16_t kIndustrialEtherType = 0x88B5;
constexpr uint32_t kIndustrialMagic = 0x584C494E; // "XLIN"
constexpr uint32_t kMaxPayloadLength = 9000;

enum class MessageType : uint8_t {
    LvdsData       = 1,
    ControlCommand = 2,
    ControlResult  = 3,
    StatusReport   = 4,
    Heartbeat      = 5,
    Ack            = 6,
    Nack           = 7,
    FlowControl    = 8,
    ErrorReport    = 9
};

#pragma pack(push, 1)
struct IndustrialHeader {
    uint32_t magic;
    uint8_t  version;
    uint8_t  header_length;
    uint8_t  message_type;
    uint8_t  flags;

    uint16_t channel_id;
    uint16_t reserved;

    uint64_t stream_sequence;
    uint64_t timestamp_ns;

    uint32_t payload_length;
    uint32_t command_id;
    uint32_t header_crc32;
};
#pragma pack(pop)

static_assert(sizeof(IndustrialHeader) == 40,
              "IndustrialHeader must be 40 bytes");

struct ParsedFrame {
    uint16_t channel_id = 0;
    uint32_t payload_length = 0;
    uint32_t command_id = 0;
    uint64_t stream_sequence = 0;
    uint64_t timestamp_ns = 0;
    uint8_t flags = 0;
    MessageType message_type = MessageType::LvdsData;
    const uint8_t* payload = nullptr;
};

uint32_t crc32(const void* data, std::size_t length);
bool parse_frame(const uint8_t* frame,
                 std::size_t frame_length,
                 ParsedFrame& output);

} // namespace industrial
