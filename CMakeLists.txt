cmake_minimum_required(VERSION 3.18)
project(magicscan VERSION 0.7 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "MagicScan now targets Windows only (C++17 + Win32).")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# A checked-in text-encoded derivative of the approved scanner artwork is
# converted to an ICO at configure time, including common small-icon sizes.
set(MAGICSCAN_ICON_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/magicscan.ico")
execute_process(
    COMMAND powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate-icon.ps1"
        -Source "${CMAKE_CURRENT_SOURCE_DIR}/assets/magicscan-icon.png.b64"
        -Output "${MAGICSCAN_ICON_OUTPUT}"
    RESULT_VARIABLE MAGICSCAN_ICON_RESULT
    ERROR_VARIABLE MAGICSCAN_ICON_ERROR
)
if(NOT MAGICSCAN_ICON_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not generate Windows icon: ${MAGICSCAN_ICON_ERROR}")
endif()
file(TO_CMAKE_PATH "${MAGICSCAN_ICON_OUTPUT}" MAGICSCAN_ICON_PATH)
# rc.exe can misinterpret UTF-8 source strings when generating VERSIONINFO.
# Emit UTF-16LE with BOM, which rc.exe reads as Unicode without code-page guessing.
configure_file(cmake/magicscan.rc.in "${CMAKE_CURRENT_BINARY_DIR}/magicscan.rc.utf8" @ONLY)
execute_process(
    COMMAND powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/write-unicode-rc.ps1" -Source
        "${CMAKE_CURRENT_BINARY_DIR}/magicscan.rc.utf8"
        -Destination "${CMAKE_CURRENT_BINARY_DIR}/magicscan.rc"
    RESULT_VARIABLE MAGICSCAN_RC_RESULT
)
if(NOT MAGICSCAN_RC_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not write UTF-16 Windows version resource")
endif()
configure_file(cmake/version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/version.hpp" @ONLY)


add_executable(magicscan
    ${CMAKE_CURRENT_BINARY_DIR}/magicscan.rc
    src/main.cpp
    src/service/client.cpp
    src/service/stream.cpp
    src/usb/probe.cpp
    src/usb/access_diagnostics.cpp
    src/usb/diagnostics.cpp
    src/usb/capture.cpp
    src/core/monitor.cpp
    src/core/settings.cpp
    src/core/image_writer.cpp
    src/gui/clipboard.cpp
    src/gui/preview.cpp
)
# GUI is deliberately a separate executable. The CLI remains suitable for
# scanner diagnostics and capture tests without a graphical shell.
add_executable(magicscan-tray WIN32
    src/gui/tray.cpp
    src/service/stream.cpp
    src/usb/access_diagnostics.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/magicscan.rc
    src/gui/about.cpp
    src/gui/preferences.cpp
    src/core/settings.cpp
    src/gui/live_preview.cpp
    src/gui/export_formats.cpp
    src/gui/clipboard.cpp
    src/core/monitor.cpp
    src/core/image_writer.cpp
    src/usb/probe.cpp
    src/usb/capture.cpp
)
# Standalone SCM process: does not interact with the desktop or clipboard.
# Only the status pipe exists in this foundation; acquisition stays in the tray.
add_executable(magicscan-usb-service
    src/service/service.cpp
    src/service/stream_server.cpp
    src/usb/capture.cpp
    src/core/image_writer.cpp
    src/usb/probe.cpp
)
target_compile_definitions(magicscan-usb-service PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(magicscan-usb-service PRIVATE advapi32 gdiplus gdi32 user32)
if(MSVC)
    target_compile_options(magicscan-usb-service PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(magicscan-usb-service PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_link_libraries(magicscan PRIVATE advapi32)
target_include_directories(magicscan-tray PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(magicscan-tray PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(magicscan-tray PRIVATE advapi32 gdiplus gdi32 user32 shell32 comdlg32 comctl32)
if(MSVC)
    target_compile_options(magicscan-tray PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(magicscan-tray PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_include_directories(magicscan PRIVATE src)
target_compile_definitions(magicscan PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(magicscan PRIVATE setupapi gdiplus comdlg32 gdi32 user32)
if(MSVC)
    target_compile_options(magicscan PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(magicscan PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
add_test(NAME help COMMAND magicscan --help)
