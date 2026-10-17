#include "probe.hpp"
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace magicscan {
namespace {
std::string win_error(DWORD code) {
    return "Windows error " + std::to_string(code);
}
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    ~Handle() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};
struct Command {
    SCSI_PASS_THROUGH_DIRECT pass{};
    ULONG filler = 0;
    UCHAR sense[32]{};
};

bool identify(HANDLE disk, std::string& error) {
    // C5 07 ... transfer length 4, FF02: observed to return ASCII NOVA.
    constexpr std::array<UCHAR, 16> cdb = {
        0xC5, 0x07, 0, 0, 0, 0, 0, 0, 0, 4, 0xFF, 0x02, 0, 0, 0, 0
    };
    alignas(16) std::array<unsigned char, 4> data{};
    Command command{};
    command.pass.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    command.pass.CdbLength = static_cast<UCHAR>(cdb.size());
    command.pass.SenseInfoLength = sizeof(command.sense);
    command.pass.DataIn = SCSI_IOCTL_DATA_IN;
    command.pass.DataTransferLength = static_cast<ULONG>(data.size());
    command.pass.TimeOutValue = 5;
    command.pass.DataBuffer = data.data();
    command.pass.SenseInfoOffset = offsetof(Command, sense);
    std::memcpy(command.pass.Cdb, cdb.data(), cdb.size());
    DWORD returned = 0;
    if (!DeviceIoControl(disk, IOCTL_SCSI_PASS_THROUGH_DIRECT, &command,
                         sizeof(command), &command, sizeof(command), &returned, nullptr)) {
        error = "IOCTL_SCSI_PASS_THROUGH_DIRECT: " + win_error(GetLastError());
        return false;
    }
    if (command.pass.ScsiStatus != 0) {
        error = "SCSI status " + std::to_string(command.pass.ScsiStatus);
        return false;
    }
    if (std::memcmp(data.data(), "NOVA", 4) != 0) {
        std::ostringstream out;
        out << "Unexpected response: ";
        for (auto byte : data) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        error = out.str();
        return false;
    }
    return true;
}
} // namespace

ProbeResult probe_scanner() {
    ProbeResult result;
    // Discover via zero-access metadata handles; ordinary accounts often cannot
    // request GENERIC_READ on a raw PhysicalDrive even for a USB scanner.
    for (int i = 0; i < 32; ++i) {
        const std::string path = "\\\\.\\PhysicalDrive" + std::to_string(i);
        Handle metadata;
        metadata.h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 nullptr, OPEN_EXISTING, 0, nullptr);
        if (metadata.h == INVALID_HANDLE_VALUE) continue;
        alignas(STORAGE_DEVICE_DESCRIPTOR) std::array<unsigned char, 1024> storage{};
        STORAGE_PROPERTY_QUERY query{};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        DWORD bytes = 0;
        if (!DeviceIoControl(metadata.h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                             storage.data(), static_cast<DWORD>(storage.size()), &bytes, nullptr)) continue;
        if (bytes < sizeof(STORAGE_DEVICE_DESCRIPTOR)) continue;
        const auto* descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(storage.data());
        auto contains = [&](DWORD offset, const char* text) {
            if (offset == 0 || offset >= bytes) return false;
            const char* start = reinterpret_cast<const char*>(storage.data() + offset);
            size_t length = 0;
            while (length < bytes - offset && start[length] != '\0') ++length;
            return std::string(start, length).find(text) != std::string::npos;
        };
        if (descriptor->BusType != BusTypeUsb ||
            !(contains(descriptor->ProductIdOffset, "MagicScan") ||
              contains(descriptor->ProductIdOffset, "MAGICSCAN"))) continue;

        result.found = true;
        result.path = path;

        // Only send NOVA after matching BOTH USB bus and MagicScan product name.
        // Try the least privileged handle first.
        std::string no_access_error;
        if (identify(metadata.h, no_access_error)) {
            result.nova = true;
            return result;
        }
        Handle reading;
        reading.h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, 0, nullptr);
        if (reading.h == INVALID_HANDLE_VALUE) {
            result.error = "MagicScan detected at " + path +
                           ", but SCSI on zero-access handle failed (" + no_access_error +
                           ") and GENERIC_READ open failed (" + win_error(GetLastError()) + ")";
            return result;
        }
        std::string read_error;
        if (identify(reading.h, read_error)) {
            result.nova = true;
            return result;
        }
        // Windows SCSI pass-through often requires both READ and WRITE
        // access flags on the handle, even for a data-IN CDB.
        Handle pass_through;
        pass_through.h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
        if (pass_through.h == INVALID_HANDLE_VALUE) {
            result.error = "MagicScan detected at " + path +
                           "; pass-through handle (GENERIC_READ|GENERIC_WRITE) open failed (" +
                           win_error(GetLastError()) + "). Zero-access SCSI [" +
                           no_access_error + "]; read-only SCSI [" + read_error + "]";
            return result;
        }
        result.nova = identify(pass_through.h, result.error);
        if (!result.nova) {
            result.error = "Read/write access granted, but read-only NOVA SCSI failed: " + result.error;
        }
        return result;
    }
    result.error = "No USB PhysicalDrive with MagicScan product ID found via zero-access metadata query.";
    return result;
}
} // namespace magicscan
#else
namespace magicscan {
ProbeResult probe_scanner() {
    return {false, false, {}, "Linux SG_IO backend is not implemented yet."};
}
} // namespace magicscan
#endif
