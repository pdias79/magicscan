#include "protocol.hpp"
#include "../usb/capture.hpp"
#include <windows.h>
#include <sddl.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace magicscan::service {
namespace {
constexpr uint32_t kBandSize = 2592u * 192u * 3u;
struct Header { uint32_t kind; uint32_t size; };
enum : uint32_t { kStart = 1, kBand = 2, kFinish = 3, kConnected = 4, kFailure = 5, kHeartbeat = 6 };

bool write_bytes(HANDLE pipe, const void* data, size_t size, HANDLE stop_event) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    while (size) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(size, 65536));
        OVERLAPPED async{};
        async.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!async.hEvent) return false;
        DWORD wrote = 0;
        BOOL ok = WriteFile(pipe, bytes, chunk, &wrote, &async);
        if (!ok && GetLastError() == ERROR_IO_PENDING) {
            HANDLE signals[] = {stop_event, async.hEvent};
            if (WaitForMultipleObjects(2, signals, FALSE, 3000) == WAIT_OBJECT_0 + 1) {
                ok = GetOverlappedResult(pipe, &async, &wrote, FALSE);
            } else {
                CancelIoEx(pipe, &async);
                GetOverlappedResult(pipe, &async, &wrote, TRUE);
                ok = FALSE;
            }
        }
        CloseHandle(async.hEvent);
        if (!ok || wrote != chunk) return false;
        bytes += chunk;
        size -= chunk;
    }
    return true;
}
bool send(HANDLE pipe, uint32_t kind, const std::vector<uint8_t>* band, HANDLE stop_event) {
    const Header header{kind, band ? static_cast<uint32_t>(band->size()) : 0};
    return write_bytes(pipe, &header, sizeof(header), stop_event) &&
           (!band || write_bytes(pipe, band->data(), band->size(), stop_event));
}
} // namespace

void run_stream(HANDLE stop_event) {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;BU)",
            SDDL_REVISION_1, &descriptor, nullptr)) return;
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};

    while (WaitForSingleObject(stop_event, 0) != WAIT_OBJECT_0) {
        HANDLE pipe = CreateNamedPipeW(kStreamPipePath,
            PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1, 65536, 65536, 1000, &attributes);
        if (pipe == INVALID_HANDLE_VALUE) break;
        OVERLAPPED connection{};
        connection.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!connection.hEvent) { CloseHandle(pipe); break; }
        const BOOL immediate = ConnectNamedPipe(pipe, &connection);
        const DWORD error = immediate ? ERROR_SUCCESS : GetLastError();
        bool connected = immediate || error == ERROR_PIPE_CONNECTED;
        if (!connected && error == ERROR_IO_PENDING) {
            HANDLE signals[] = {stop_event, connection.hEvent};
            if (WaitForMultipleObjects(2, signals, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
                DWORD transferred = 0;
                connected = GetOverlappedResult(pipe, &connection, &transferred, FALSE) != FALSE;
            } else {
                CancelIoEx(pipe, &connection);
                WaitForSingleObject(connection.hEvent, INFINITE);
            }
        }
        CloseHandle(connection.hEvent);

        if (connected && WaitForSingleObject(stop_event, 0) != WAIT_OBJECT_0) {
            std::atomic_bool cancel{false};
            auto last_pulse = std::chrono::steady_clock::now();
            ScanEvents events{};
            events.on_start = [&] { if (!send(pipe, kStart, nullptr, stop_event)) cancel = true; };
            events.on_connected = [&] { if (!send(pipe, kConnected, nullptr, stop_event)) cancel = true; };
            events.on_band = [&](const std::vector<uint8_t>& pixels, uint32_t w, uint32_t h) {
                if (w != 2592 || h != 192 || pixels.size() != kBandSize ||
                    !send(pipe, kBand, &pixels, stop_event)) cancel = true;
            };
            events.on_complete = [&](const std::wstring&) {
                if (!send(pipe, kFinish, nullptr, stop_event)) cancel = true;
            };
            events.on_tick = [&] {
                if (WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0) cancel = true;
                const auto now = std::chrono::steady_clock::now();
                if (!cancel && now - last_pulse >= std::chrono::seconds(1)) {
                    if (!send(pipe, kHeartbeat, nullptr, stop_event)) cancel = true;
                    last_pulse = now;
                }
            };
            while (!cancel && WaitForSingleObject(stop_event, 0) != WAIT_OBJECT_0) {
                std::string error_text;
                if (!capture_page(L"", error_text, true, &cancel, &events)) {
                    if (cancel) break;
                    if (!send(pipe, kFailure, nullptr, stop_event)) break;
                    for (int n = 0; n < 10 && !cancel &&
                           WaitForSingleObject(stop_event, 0) != WAIT_OBJECT_0; ++n)
                        Sleep(100);
                }
            }
            DisconnectNamedPipe(pipe);
        }
        CloseHandle(pipe);
    }
    LocalFree(descriptor);
}
} // namespace magicscan::service
