#include "dpdk_helpers.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <string>

namespace industrial {

bool write_wal_record(
    const std::string& wal_dir,
    uint16_t channel_id,
    const void* data,
    std::size_t data_len) {
    if (data == nullptr || data_len == 0) {
        return false;
    }

    std::filesystem::create_directories(wal_dir);

    const std::string path =
        wal_dir + "/channel_" + std::to_string(channel_id) + ".wal";

    const int fd = ::open(
        path.c_str(),
        O_WRONLY | O_CREAT | O_APPEND,
        0644);

    if (fd < 0) {
        return false;
    }

    const char* source = static_cast<const char*>(data);
    std::size_t written_total = 0;

    while (written_total < data_len) {
        const ssize_t written = ::write(
            fd,
            source + written_total,
            data_len - written_total);

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }

            ::close(fd);
            return false;
        }

        if (written == 0) {
            ::close(fd);
            return false;
        }

        written_total += static_cast<std::size_t>(written);
    }

    ::close(fd);
    return true;
}

} // namespace industrial
