#include "capture.hpp"
#include "probe.hpp"
#include "../core/image_writer.hpp"
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <limits>
#include <memory>
#include <iostream>
#include <string>
#include <vector>

namespace magicscan {
namespace {
constexpr uint32_t kWidth = 2592;
constexpr uint32_t kLines = 192;
constexpr uint32_t kMaxChunk = 65536;
constexpr uint32_t kBandSize = kWidth * kLines * 3;

struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct ScsiCommand {
    SCSI_PASS_THROUGH_DIRECT pass{};
    ULONG filler = 0;
    UCHAR sense[32]{};
};
uint32_t read_le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
void store_be32(uint8_t* p, uint32_t value) {
    p[0] = static_cast<uint8_t>(value >> 24);
    p[1] = static_cast<uint8_t>(value >> 16);
    p[2] = static_cast<uint8_t>(value >> 8);
    p[3] = static_cast<uint8_t>(value);
}
bool read_scsi(HANDLE device, const std::array<uint8_t,16>& cdb,
               uint8_t* output, uint32_t length, std::string& error) {
    ScsiCommand cmd{};
    cmd.pass.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    cmd.pass.CdbLength = 16;
    cmd.pass.SenseInfoLength = sizeof(cmd.sense);
    cmd.pass.SenseInfoOffset = offsetof(ScsiCommand, sense);
    cmd.pass.DataIn = SCSI_IOCTL_DATA_IN;
    cmd.pass.DataTransferLength = length;
    cmd.pass.TimeOutValue = 8;
    cmd.pass.DataBuffer = output;
    std::memcpy(cmd.pass.Cdb, cdb.data(), cdb.size());
    DWORD returned = 0;
    if (!DeviceIoControl(device, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                         &cmd, sizeof(cmd), &cmd, sizeof(cmd), &returned, nullptr)) {
        error = "SCSI passthrough Windows error " + std::to_string(GetLastError());
        return false;
    }
    if (cmd.pass.ScsiStatus != 0 || cmd.pass.DataTransferLength != length) {
        error = "SCSI failure or short transfer (status " + std::to_string(cmd.pass.ScsiStatus) + ")";
        return false;
    }
    return true;
}
std::array<uint8_t,16> meta(uint8_t length, uint8_t a, uint8_t b, uint8_t c = 0) {
    return {0xC5,0x07,0,0,0,0,0,0,0,length,a,b,c,0,0,0};
}
bool read_band(HANDLE device, uint32_t address, std::vector<uint8_t>& page,
               std::string& error) {
    if (uint64_t(address) + kBandSize > std::numeric_limits<uint32_t>::max()) {
        error = "Scanner memory address is outside 32-bit range"; return false;
    }
    alignas(16) std::array<uint8_t,16> a{},b{};
    if (!read_scsi(device, meta(0x10,0xFF,0x04), a.data(), 16, error) ||
        !read_scsi(device, meta(0x10,0xFF,0x06), b.data(), 16, error))
        return false;
    std::vector<uint8_t> planar(kBandSize);
    alignas(16) std::array<uint8_t,kMaxChunk> scratch{};
    for (uint32_t offset = 0; offset < kBandSize;) {
        const uint32_t count = std::min(kMaxChunk, kBandSize - offset);
        std::array<uint8_t,16> cdb{};
        cdb[0] = 0xC3; cdb[1] = 0x07;
        store_be32(cdb.data()+2, address+offset);
        store_be32(cdb.data()+6, count);
        if (!read_scsi(device,cdb,scratch.data(),count,error)) return false;
        std::memcpy(planar.data()+offset,scratch.data(),count);
        offset += count;
    }
    // Convert each planar RGB scanline to mirrored, interleaved BGR for GDI+.
    const size_t initial = page.size();
    page.resize(initial + kBandSize);
    for (uint32_t y=0; y<kLines; ++y) {
        const uint8_t* row = planar.data()+static_cast<size_t>(y)*kWidth*3;
        uint8_t* dst = page.data()+initial+static_cast<size_t>(y)*kWidth*3;
        for (uint32_t x=0; x<kWidth; ++x) {
            const uint32_t src=kWidth-1-x;
            dst[x*3] = row[kWidth*2+src];
            dst[x*3+1] = row[kWidth+src];
            dst[x*3+2] = row[src];
        }
    }
    return true;
}
bool poll_band(HANDLE device, uint32_t& ready, uint32_t& address, std::string& error) {
    alignas(16) std::array<uint8_t,52> first{}, last{};
    alignas(16) std::array<uint8_t,16> middle{};
    if (!read_scsi(device, meta(0x34,0x02,0x01,0x01), first.data(),52,error) ||
        !read_scsi(device, meta(0x10,0xFF,0x04),middle.data(),16,error) ||
        !read_scsi(device, meta(0x34,0x02,0x02,0x01),last.data(),52,error))
        return false;
    ready = read_le32(last.data());
    address = read_le32(last.data()+20);
    if (ready == 1 && (read_le32(last.data()+12)!=kWidth ||
                       read_le32(last.data()+16)!=kLines)) {
        error="Unexpected image dimensions in scanner metadata";
        return false;
    }
    return true;
}
bool poll_button(HANDLE device, bool& scanning, std::string& error) {
    alignas(16) std::array<uint8_t,16> extra{};
    alignas(16) std::array<uint8_t,4> status{};
    // The status inquiry must be issued in its established context.
    const std::array<std::array<uint8_t,16>,9> commands = {{
        meta(0x10,0xFF,0x04), meta(0x10,0xFF,0x03),
        meta(0x10,0xFF,0x03), meta(0x10,0xFF,0x05),
        meta(0x04,0x02,0x01), meta(0x10,0xFF,0x04),
        meta(0x04,0x02,0x02), meta(0x10,0xFF,0x04),
        meta(0x10,0xFF,0x06)
    }};
    for (size_t i=0;i<commands.size();++i) {
        const bool short_reply = i == 4 || i == 6;
        if (!read_scsi(device,commands[i],short_reply ? status.data() : extra.data(),
                       short_reply ? 4 : 16,error)) return false;
        if (i==6) scanning=read_le32(status.data())==1;
    }
    return true;
}
} // namespace

bool capture_first_band(const std::wstring& output_png, std::string& error) {
    auto result=probe_scanner();
    if (!result.found || !result.nova) {
        error="Scanner identification failed: " + result.error; return false;
    }
    Handle device;
    // NOVA was validated against the USB MagicScan device.
    // The handle requests read/write *permissions* because SCSI pass-through
    // may require both; only DATA_IN CDBs are sent, never filesystem writes.
    device.value=CreateFileA(result.path.c_str(),GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    if (device.value==INVALID_HANDLE_VALUE) {
        error="Unable to open scanner: Windows error " + std::to_string(GetLastError()); return false;
    }
    // An image must become ready *after* we have seen not-ready at least once.
    bool seen_not_ready=false;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    alignas(16) std::array<uint8_t,52> a{},c{};
    alignas(16) std::array<uint8_t,16> b{},before{},after{};
    uint32_t address=0;
    while (std::chrono::steady_clock::now()<deadline) {
        if (!read_scsi(device.value,meta(0x34,0x02,0x01,0x01),a.data(),52,error) ||
            !read_scsi(device.value,meta(0x10,0xFF,0x04),b.data(),16,error) ||
            !read_scsi(device.value,meta(0x34,0x02,0x02,0x01),c.data(),52,error))
            return false;
        uint32_t ready=read_le32(c.data());
        uint32_t width=read_le32(c.data()+12);
        uint32_t lines=read_le32(c.data()+16);
        uint32_t candidate=read_le32(c.data()+20);
        if (ready==0) seen_not_ready=true;
        if (seen_not_ready && ready==1 && width==kWidth && lines==kLines && candidate!=0) {
            address=candidate;
            break;
        }
        Sleep(100);
    }
    if (!address) {
        error="Timed out waiting for a fresh 2592x192 band (start a scan after invoking command)";
        return false;
    }
    if (uint64_t(address)+kBandSize>std::numeric_limits<uint32_t>::max()) {
        error="Out-of-range scanner memory address"; return false;
    }
    if (!read_scsi(device.value,meta(0x10,0xFF,0x04),before.data(),16,error) ||
        !read_scsi(device.value,meta(0x10,0xFF,0x06),after.data(),16,error))
        return false;
    std::vector<uint8_t> strip(kBandSize);
    alignas(16) std::array<uint8_t,kMaxChunk> chunk{};
    for (uint32_t offset=0;offset<kBandSize;) {
        uint32_t amount=std::min(kMaxChunk,kBandSize-offset);
        std::array<uint8_t,16> cdb{};
        cdb[0]=0xC3; cdb[1]=0x07;
        store_be32(cdb.data()+2,address+offset);
        store_be32(cdb.data()+6,amount);
        if (!read_scsi(device.value,cdb,chunk.data(),amount,error)) return false;
        std::memcpy(strip.data()+offset,chunk.data(),amount);
        offset+=amount;
    }
    return write_png(output_png,strip,error);
}

bool capture_page(const std::wstring& output_png, std::string& error,
                  bool wait_for_start_forever, const std::atomic_bool* stop,
                  const ScanEvents* events) {
    const auto scanner = probe_scanner();
    if (!scanner.found || !scanner.nova) {
        error = "Scanner identification failed: " + scanner.error;
        return false;
    }
    Handle device;
    device.value = CreateFileA(scanner.path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (device.value == INVALID_HANDLE_VALUE) {
        error = "Scanner open failed: Windows error " + std::to_string(GetLastError());
        return false;
    }
    if (events && events->on_connected) events->on_connected();
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    // Generous emergency guard, not a page-height limit.
    auto deadline = started + std::chrono::minutes(5);
    auto last_band = started;
    auto previous_active = started;
    bool active = false;
    bool seen_idle = false;
    bool band_was_ready = false;
    bool session_started = false;
    bool stop_seen = false;
    uint32_t last_address = 0;
    uint32_t last_ready_state = 2; // Unknown initial state.
    uint64_t idle_observations = 0;
    uint64_t ready_edges = 0;
    std::vector<uint8_t> pixels;
    constexpr size_t kMaxPageBytes = 256ull * 1024 * 1024;
    std::cout << "Ready. Press SCAN to begin, press SCAN again to finish.\n" << std::flush;
    while ((wait_for_start_forever && !session_started) || Clock::now() < deadline) {
        if (stop && stop->load()) {
            error = "Scan monitoring stopped";
            return false;
        }
        bool scanning = false;
        if (!poll_button(device.value, scanning, error)) return false;
        if (scanning && !active) {
            session_started = true;
            deadline = Clock::now() + std::chrono::minutes(5);
            stop_seen = false;
            std::cout << "Scan started\n" << std::flush;
            if (events && events->on_start) events->on_start();
        }
        if (!scanning && active && session_started) {
            stop_seen = true;
            previous_active = Clock::now();
            std::cout << "Scan stopped; draining pending bands\n" << std::flush;
        }
        active = scanning;

        uint32_t ready=0, address=0;
        if (!poll_band(device.value,ready,address,error)) return false;
        if (ready != last_ready_state) {
            const std::string edge = "MagicScan ready transition " +
                std::to_string(last_ready_state) + " -> " +
                std::to_string(ready) + " at scanner address " +
                std::to_string(address) + "\n";
            OutputDebugStringA(edge.c_str());
            if (ready == 1) ++ready_edges;
            last_ready_state = ready;
        }
        if (ready == 0) ++idle_observations;
        if (ready==0) {
            seen_idle = true;
            band_was_ready = false;
        }
        // Avoid reading the same ready state repeatedly. A changed non-zero
        // address is treated as new, but equal address requires a ready=0 edge.
        if (ready==1 && address!=0 && seen_idle && session_started &&
            (!band_was_ready || address!=last_address)) {
            if (pixels.size()+kBandSize>kMaxPageBytes) {
                error="Page exceeds 256 MiB safety memory budget"; return false;
            }
            const uint32_t previous_address = last_address;
            const size_t offset_before = pixels.size();
            const auto read_started = Clock::now();
            if (!read_band(device.value,address,pixels,error)) return false;
            const auto read_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now() - read_started).count();
            if (events && events->on_band) {
                std::vector<uint8_t> band(pixels.begin() + static_cast<ptrdiff_t>(offset_before), pixels.end());
                events->on_band(band, kWidth, kLines);
            }
            last_band=Clock::now();
            last_address=address;
            band_was_ready=true;
            // Record source order and hardware memory address for diagnosing
            // fast scans. The address may wrap/reuse; do not sort by address
            // without establishing the scanner's ordering semantics.
            const auto band_number = pixels.size() / kBandSize;
            std::cout << "Band " << band_number << " read at address "
                      << address << "\n" << std::flush;
            const std::string trace = "MagicScan band " +
                std::to_string(band_number) + " scanner address " +
                std::to_string(address) + " read_ms " +
                std::to_string(read_ms) + " delta_address " +
                std::to_string(static_cast<int64_t>(address) -
                               static_cast<int64_t>(previous_address)) + "\n";
            OutputDebugStringA(trace.c_str());
        }
        if (stop_seen && !pixels.empty() &&
            Clock::now()-std::max(last_band,previous_active)>=std::chrono::seconds(2))
            break;
        // An intentional zero-band scan is a normal completed session.
        // Keep a short drain window for late strips in empty scans; nonempty scans retain two seconds.
        if (stop_seen && pixels.empty() &&
            Clock::now()-previous_active>=std::chrono::milliseconds(300)) {
            if (events && events->on_complete) events->on_complete(L"");
            std::cout << "Scan finished without image bands.\n" << std::flush;
            return true;
        }
        if (events && events->on_tick) events->on_tick();
        // Poll the NOVA ready/address metadata promptly between full-band
        // reads. At fast hand movement a 60 ms idle interval can miss a
        // transient ready/address change. Keep the USB transfers synchronous:
        // no additional threads or sleeps inside the band read itself.
        Sleep(10);
    }
    if (pixels.empty()) {
        error="Timed out without capturing any image strips"; return false;
    }
    if (!stop_seen) {
        error="Scan did not complete within the capture timeout; no truncated PNG was saved";
        return false;
    }
    const std::string stats = "MagicScan capture summary bands " +
        std::to_string(pixels.size()/kBandSize) +
        " ready_edges " + std::to_string(ready_edges) +
        " idle_observations " + std::to_string(idle_observations) + "\n";
    OutputDebugStringA(stats.c_str());
    const size_t bands=pixels.size()/kBandSize;
    // Empty output path explicitly means memory-only capture (no disk writes).
    if (!output_png.empty() && !write_page_png(output_png,pixels,bands,error)) return false;
    if (events && events->on_complete) events->on_complete(output_png);
    std::cout << "Captured " << bands << " bands (" << kWidth << "x" << bands*kLines
              << (output_png.empty() ? ") in memory\n" : ") to PNG\n");
    return true;
}
} // namespace magicscan
