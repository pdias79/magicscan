#include "capture.hpp"
#include "probe.hpp"
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cstddef>
#include <limits>
#include <memory>
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
bool png_codec(CLSID& clsid) {
    UINT count=0, size=0;
    if (Gdiplus::GetImageEncodersSize(&count,&size) != Gdiplus::Ok || !size) return false;
    std::vector<uint8_t> storage(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
    if (Gdiplus::GetImageEncoders(count,size,codecs) != Gdiplus::Ok) return false;
    for (UINT i=0;i<count;++i) {
        if (codecs[i].MimeType && std::wcscmp(codecs[i].MimeType,L"image/png")==0) {
            clsid=codecs[i].Clsid; return true;
        }
    }
    return false;
}
bool write_png(const std::wstring& file, const std::vector<uint8_t>& planar, std::string& error) {
    Gdiplus::GdiplusStartupInput input{};
    ULONG_PTR token=0;
    if (Gdiplus::GdiplusStartup(&token,&input,nullptr) != Gdiplus::Ok) {
        error="Cannot initialize GDI+"; return false;
    }
    bool success=false;
    {
        Gdiplus::Bitmap bitmap(kWidth,kLines,PixelFormat24bppRGB);
        Gdiplus::Rect rect(0,0,kWidth,kLines);
        Gdiplus::BitmapData bits{};
        if (bitmap.LockBits(&rect,Gdiplus::ImageLockModeWrite,PixelFormat24bppRGB,&bits)==Gdiplus::Ok) {
            for (uint32_t y=0;y<kLines;++y) {
                auto* dst = static_cast<uint8_t*>(bits.Scan0) + static_cast<ptrdiff_t>(y)*bits.Stride;
                const auto* row = planar.data() + static_cast<size_t>(y)*kWidth*3;
                for (uint32_t x=0;x<kWidth;++x) {
                    uint32_t source=kWidth-1-x;
                    dst[x*3+0]=row[kWidth*2+source]; // BGR expected by GDI+
                    dst[x*3+1]=row[kWidth+source];
                    dst[x*3+2]=row[source];
                }
            }
            bitmap.UnlockBits(&bits);
            CLSID clsid{};
            if (png_codec(clsid) && bitmap.Save(file.c_str(),&clsid)==Gdiplus::Ok) success=true;
            else error="Failed to encode or save PNG";
        } else error="GDI+ LockBits failed";
    }
    Gdiplus::GdiplusShutdown(token);
    return success;
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
} // namespace magicscan
