#include "protocol.hpp"
#include "../usb/probe.hpp"
#include <windows.h>
#include <sddl.h>

namespace {
SERVICE_STATUS_HANDLE status_handle = nullptr;
SERVICE_STATUS status{};
HANDLE stop_event = nullptr;

void report(DWORD state, DWORD error = NO_ERROR) {
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP : 0;
    status.dwWin32ExitCode = error;
    status.dwWaitHint = state == SERVICE_STOP_PENDING ? 5000 : 0;
    if (status_handle) SetServiceStatus(status_handle, &status);
}

void WINAPI control(DWORD code) {
    if (code == SERVICE_CONTROL_STOP && status.dwCurrentState == SERVICE_RUNNING) {
        report(SERVICE_STOP_PENDING);
        if (stop_event) SetEvent(stop_event);
    }
}

void serve() {
    // SYSTEM and Administrators control the pipe. Built-in Users receive
    // only FILE_GENERIC_READ; they cannot write commands to the service.
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;BU)",
            SDDL_REVISION_1, &descriptor, nullptr)) return;
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    while (WaitForSingleObject(stop_event, 0) != WAIT_OBJECT_0) {
        HANDLE pipe = CreateNamedPipeW(magicscan::service::kPipePath,
            PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1, 128, 128, 1000, &attributes);
        if (pipe == INVALID_HANDLE_VALUE) break;

        OVERLAPPED overlap{};
        overlap.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!overlap.hEvent) { CloseHandle(pipe); break; }

        const BOOL connected = ConnectNamedPipe(pipe, &overlap);
        const DWORD error = connected ? ERROR_SUCCESS : GetLastError();
        bool client = connected || error == ERROR_PIPE_CONNECTED;
        if (!client && error == ERROR_IO_PENDING) {
            HANDLE events[] = {stop_event, overlap.hEvent};
            const DWORD wait = WaitForMultipleObjects(2, events, FALSE, INFINITE);
            if (wait == WAIT_OBJECT_0 + 1) {
                DWORD transferred = 0;
                client = GetOverlappedResult(pipe, &overlap, &transferred, FALSE) != FALSE;
            } else {
                CancelIoEx(pipe, &overlap);
                WaitForSingleObject(overlap.hEvent, INFINITE);
            }
        }
        CloseHandle(overlap.hEvent);
        if (client && WaitForSingleObject(stop_event, 0) != WAIT_OBJECT_0) {
            // This is identification only; no capture or file writes.
            const char reply = magicscan::probe_scanner().nova
                ? magicscan::service::kReady : magicscan::service::kUnavailable;
            DWORD written = 0;
            WriteFile(pipe, &reply, 1, &written, nullptr);
            FlushFileBuffers(pipe);
            DisconnectNamedPipe(pipe);
        }
        CloseHandle(pipe);
    }
    LocalFree(descriptor);
}

void WINAPI main_service(DWORD, LPWSTR*) {
    status_handle = RegisterServiceCtrlHandlerW(magicscan::service::kServiceName, control);
    if (!status_handle) return;
    report(SERVICE_START_PENDING);
    stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stop_event) { report(SERVICE_STOPPED, GetLastError()); return; }
    report(SERVICE_RUNNING);
    serve();
    CloseHandle(stop_event);
    stop_event = nullptr;
    report(SERVICE_STOPPED);
}
} // namespace

int wmain() {
    SERVICE_TABLE_ENTRYW entries[] = {
        {const_cast<LPWSTR>(magicscan::service::kServiceName), main_service},
        {nullptr, nullptr}
    };
    return StartServiceCtrlDispatcherW(entries) ? 0 : 1;
}
