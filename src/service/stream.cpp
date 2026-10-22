#include "stream.hpp"
#include "protocol.hpp"
#include "../core/image_writer.hpp"
#include <windows.h>
#include <chrono>
#include <algorithm>
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
enum : uint32_t { kStart = 1, kBand = 2, kFinish = 3, kConnected = 4, kFailure = 5, kHeartbeat = 6, kSequencedBand = 7 };
bool read_exact(HANDLE pipe, void* data, DWORD length, const std::atomic_bool* stop,
                bool wait_for_header = false) {
    // Poll only between messages, never for image payloads: polling each
    // fragment delays large bands and can backpressure the scanner.
    auto* out = static_cast<unsigned char*>(data);
    DWORD done = 0;
    auto last_progress = std::chrono::steady_clock::now();
    while (done < length && (!stop || !stop->load())) {
        DWORD amount = length - done;
        if (wait_for_header) {
            DWORD available = 0;
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
                return false;
            if (available == 0) {
                if (std::chrono::steady_clock::now() - last_progress >
                    std::chrono::seconds(10)) return false;
                Sleep(50);
                continue;
            }
            amount = std::min<DWORD>(amount, available);
        }
        DWORD received = 0;
        if (!ReadFile(pipe, out + done, amount, &received, nullptr) || !received)
            return false;
        done += received;
        last_progress = std::chrono::steady_clock::now();
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
    if (!WaitNamedPipeW(kStreamPipePath, 2000)) return -1;
    HANDLE pipe = CreateFileW(kStreamPipePath, GENERIC_READ, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return -1;
    // Set bounded synchronous reads so Pause/Quit cannot hang indefinitely.
    // Cancellation from another thread uses CancelSynchronousIo on worker thread.
    std::vector<uint8_t> page;
    uint32_t expected_band = 0;
    bool scan_started = false;
    if (events && events->on_connected) events->on_connected();
    while (!stop || !stop->load()) {
        Header head{};
        if (!read_exact(pipe, &head, sizeof(head), stop, true)) { break; }
        if ((head.kind == kStart || head.kind == kFinish || head.kind == kConnected ||
             head.kind == kFailure || head.kind == kHeartbeat) && head.size != 0) { break; }
        if (head.kind == kStart) {
            page.clear();
            expected_band = 0;
            scan_started = true;
            if (events && events->on_start) events->on_start();
        } else if ((head.kind == kSequencedBand && head.size == kBytes + sizeof(uint32_t)) ||
                   (head.kind == kBand && head.size == kBytes)) {
            if (!scan_started || page.size() > kMaxPageBytes - kBytes) break;
            if (head.kind == kSequencedBand) {
                uint32_t received_sequence = 0;
                if (!read_exact(pipe, &received_sequence, sizeof(received_sequence), stop))
                    break;
                // Don't silently accept a skipped or duplicate image strip.
                if (received_sequence != expected_band) break;
            }
            ++expected_band;
            const size_t offset = page.size();
            page.resize(offset + kBytes);
            if (!read_exact(pipe, page.data() + offset, kBytes, stop)) { break; }
            if (events && events->on_band) {
                std::vector<uint8_t> band(page.begin() + static_cast<ptrdiff_t>(offset), page.end());
                events->on_band(band, kWidth, kRows);
            }
        } else if (head.kind == kFinish && page.size() % kBytes == 0) {
            std::wstring filename;
            if (auto_save && !page.empty()) {
                filename = unique_path(directory);
                std::string error;
                if (filename.empty() || !write_page_png(filename, page, page.size() / kBytes, error)) {
                    break;
                }
            }
            if (events && events->on_complete) events->on_complete(filename);
            page.clear();
            scan_started = false;
        } else if (head.kind == kHeartbeat) {
            continue;
        } else if (head.kind == kConnected) {
            if (events && events->on_connected) events->on_connected();
        } else if (head.kind == kFailure) {
            if (events && events->on_disconnected) events->on_disconnected();
            page.clear();
            scan_started = false;
        } else {
            break;
        }
    }
    CloseHandle(pipe);
    if ((!stop || !stop->load()) && events && events->on_disconnected) events->on_disconnected();
    return (stop && stop->load()) ? 0 : 1;
}
}
