#include "probe.hpp"
#include <windows.h>
#include <winioctl.h>
#include <array>
#include <iostream>
#include <string>

namespace magicscan {
namespace {
std::string descriptor_text(const unsigned char* bytes, DWORD total, DWORD offset) {
    if (offset == 0 || offset >= total) return "<absent>";
    DWORD length = 0;
    while (length < total - offset && bytes[offset + length] != 0) ++length;
    return std::string(reinterpret_cast<const char*>(bytes + offset), length);
}
}
void list_disk_devices() {
    std::cout << "Windows disk enumeration (device metadata only; no SCSI vendor commands)\n";
    std::cout << "Disk | Open(metadata) | Bus | Vendor | Product | Serial\n";
    for (int index = 0; index < 32; ++index) {
        const std::string path = "\\\\.\\PhysicalDrive" + std::to_string(index);
        // Request zero access rights. Storage descriptor inquiries can succeed
        // even when read/write SCSI access would require elevation.
        HANDLE handle = CreateFileA(path.c_str(), 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            const DWORD code = GetLastError();
            if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND &&
                code != ERROR_INVALID_NAME) {
                std::cout << path << " | Open error " << code << "\n";
            }
            continue;
        }
        STORAGE_PROPERTY_QUERY query{};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        alignas(STORAGE_DEVICE_DESCRIPTOR) std::array<unsigned char,2048> buffer{};
        DWORD bytes = 0;
        const BOOL ok = DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY,
            &query, sizeof(query), buffer.data(), static_cast<DWORD>(buffer.size()), &bytes, nullptr);
        const DWORD query_error = ok ? ERROR_SUCCESS : GetLastError();
        CloseHandle(handle);
        if (!ok) {
            std::cout << path << " | Query error " << query_error << "\n";
            continue;
        }
        if (bytes < sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
            std::cout << path << " | Short descriptor\n";
            continue;
        }
        const auto* d = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
        std::cout << path << " | OK | " << static_cast<int>(d->BusType)
                  << " | " << descriptor_text(buffer.data(), bytes, d->VendorIdOffset)
                  << " | " << descriptor_text(buffer.data(), bytes, d->ProductIdOffset)
                  << " | " << descriptor_text(buffer.data(), bytes, d->SerialNumberOffset)
                  << "\n";
    }
    std::cout << "Tip: bus 7=USB; other bus values may vary. "
                 "Do not use a random PhysicalDrive for SCSI vendor commands.\n";
}
} // namespace magicscan
