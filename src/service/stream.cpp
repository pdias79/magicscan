#include "stream.hpp"
#include "protocol.hpp"
#include "../core/image_writer.hpp"
#include <windows.h>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>
#include <cstring>

namespace magicscan::service {
namespace {
constexpr uint32_t kWidth = 2592, kRows = 192;
constexpr uint32_t kBytes = kWidth * kRows * 3;
constexpr size_t kMaxPageBytes = 256ull * 1024 * 1024;
struct Header { uint32_t kind; uint32_t size; };
enum : uint32_t { kStart = 1, kBand = 2, kFinish = 3, kConnected = 4, kFailure = 5, kHeartbeat = 6 };
bool read_exact(HANDLE pipe, void* data, DWORD length, const std::atomic_bool* stop) {
    auto* out = static_cast<unsigned char*>(data);
    DWORD done = 0;
    while (done < length && (!stop || !stop->load())) {
        DWORD received = 0;
        if (!ReadFile(pipe, out + done, length - done, &received, nullptr) || !received) return false;
        done += received;
    }
    return done == length;
}
std::wstring unique_path(const std::filesystem::path& folder) {
    SYSTEMTIME clock{};
    GetLocalTime(&clock);
    std::wostringstream base;
    base << L"scan_" << std::setfill(L'0') << std::setw(4) << clock.wYear
         << std::setw(2) << clock.wMonth << std::setw(2) << clock.wDay << L"_"
         << std::setw(2) << clock.wHour << std::setw(2) << clock.wMinute
         << std::setw(2) << clock.wSecond << L"_" << std::setw(3) << clock.wMilliseconds;
    for (unsigned i = 0; i < 10000; ++i) {
        const auto path = folder / (base.str() + (i ? L"_" + std::to_wstring(i) : L"") + L".png");
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) && !ec) return path.wstring();
    }
    return {};
}
} // namespace

int monitor_remote(const std::wstring& directory, bool auto_save,
                    std::atomic_bool* stop, const ScanEvents* events) {
    if (!WaitNamedPipeW(kStreamPipePath, 2000)) return false;
    HANDLE pipe = CreateFileW(kStreamPipePath, GENERIC_READ, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return false;
    // Set bounded synchronous reads so Pause/Quit cannot hang indefinitely.
    // Cancellation from another thread uses CancelSynchronousIo on worker thread.
    bool success = true;
    std::vector<uint8_t> page;
    if (events && events->on_connected) events->on_connected();
    while (!stop || !stop->load()) {
        Header head{};
        if (!read_exact(pipe, &head, sizeof(head), stop)) { success = false; break; }
        if ((head.kind == kStart || head.kind == kFinish || head.kind == kConnected ||
             head.kind == kFailure || head.kind == kHeartbeat) && head.size != 0) { success = false; break; }
        if (head.kind == kStart) {
            page.clear();
            if (events && events->on_start) events->on_start();
        } else if (head.kind == kBand && head.size == kBytes &&
                   page.size() <= kMaxPageBytes - kBytes) {
            const size_t offset = page.size();
            page.resize(offset + kBytes);
            if (!read_exact(pipe, page.data() + offset, kBytes, stop)) { success = false; break; }
            if (events && events->on_band) {
                std::vector<uint8_t> band(page.begin() + static_cast<ptrdiff_t>(offset), page.end());
                events->on_band(band, kWidth, kRows);
            }
        } else if (head.kind == kFinish && !page.empty() && page.size() % kBytes == 0) {
            std::wstring filename;
            if (auto_save) {
                filename = unique_path(directory);
                std::string error;
                if (filename.empty() || !write_page_png(filename, page, page.size() / kBytes, error)) {
                    success = false;
                    break;
                }
            }
            if (events && events->on_complete) events->on_complete(filename);
            page.clear();
        } else if (head.kind == kHeartbeat) {
            continue;
        } else if (head.kind == kConnected) {
            if (events && events->on_connected) events->on_connected();
        } else if (head.kind == kFailure) {
            if (events && events->on_disconnected) events->on_disconnected();
            page.clear();
        } else {
            success = false;
            break;
        }
    }
    CloseHandle(pipe);
    if (!success && events && events->on_disconnected) events->on_disconnected();
    return (stop && stop->load()) ? 0 : 1;
}
}
