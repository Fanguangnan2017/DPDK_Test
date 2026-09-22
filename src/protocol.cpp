#include "protocol.hpp"

#include <arpa/inet.h>
#include <cstring>

namespace industrial {

uint32_t crc32(const void* data, std::size_t length) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFFu;

    for (std::size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];

        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 1U) {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            } else {
                crc >>= 1U;
            }
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

bool parse_frame(const uint8_t* frame,
                 std::size_t frame_length,
                 ParsedFrame& output) {
    constexpr std::size_t eth_header_len = 14;

    if (frame == nullptr) {
        return false;
    }

    if (frame_length < eth_header_len + sizeof(IndustrialHeader)) {
        return false;
    }

    uint16_t ether_type = 0;
    std::memcpy(&ether_type, frame + 12, sizeof(ether_type));
    ether_type = ntohs(ether_type);

    if (ether_type != kIndustrialEtherType) {
        return false;
    }

    auto* header = reinterpret_cast<const IndustrialHeader*>(
        frame + eth_header_len);

    if (ntohl(header->magic) != kIndustrialMagic) {
        return false;
    }

    if (header->header_length != sizeof(IndustrialHeader)) {
        return false;
    }

    const auto type = static_cast<MessageType>(header->message_type);
    if (static_cast<uint8_t>(type) < 1 ||
        static_cast<uint8_t>(type) > 9) {
        return false;
    }

    const uint16_t channel_id = ntohs(header->channel_id);
    if (channel_id >= kChannelCount) {
        return false;
    }

    const uint32_t payload_length = ntohl(header->payload_length);
    if (payload_length == 0 || payload_length > kMaxPayloadLength) {
        return false;
    }

    const std::size_t required_len =
        eth_header_len + sizeof(IndustrialHeader) + payload_length;

    if (frame_length < required_len) {
        return false;
    }

    IndustrialHeader header_copy = *header;
    const uint32_t received_crc = ntohl(header_copy.header_crc32);
    header_copy.header_crc32 = 0;

    const uint32_t calc_crc = crc32(&header_copy, sizeof(header_copy));
    if (received_crc != calc_crc) {
        return false;
    }

    output.channel_id = channel_id;
    output.payload_length = payload_length;
    output.message_type = type;
    output.payload = frame + eth_header_len + sizeof(IndustrialHeader);

    return true;
}

} // namespace industrial
