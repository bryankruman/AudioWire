// SPDX-License-Identifier: MIT
#include <windows.h>
#include <iostream>
#include <newdev.h>
#include <setupapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
static const GUID media = {0x4d36e96c, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};
static bool isAudioWire(HDEVINFO set, SP_DEVINFO_DATA& device) {
    WCHAR ids[4096]{};
    DWORD type = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(set, &device, SPDRP_HARDWAREID, &type,
                                           reinterpret_cast<BYTE*>(ids), sizeof(ids), nullptr) ||
        type != REG_MULTI_SZ)
        return false;
    for (auto p = ids; *p; p += wcslen(p) + 1)
        if (_wcsicmp(p, L"ROOT\\AudioWire") == 0)
            return true;
    return false;
}
static int install(const std::wstring& inf) {
    BOOL reboot = FALSE;
    if (UpdateDriverForPlugAndPlayDevicesW(nullptr, L"ROOT\\AudioWire", inf.c_str(), INSTALLFLAG_FORCE,
                                           &reboot))
        return reboot ? 3010 : 0;
    if (GetLastError() != ERROR_NO_SUCH_DEVINST)
        return static_cast<int>(GetLastError());
    HDEVINFO set = SetupDiCreateDeviceInfoList(&media, nullptr);
    if (set == INVALID_HANDLE_VALUE)
        return static_cast<int>(GetLastError());
    SP_DEVINFO_DATA device{};
    device.cbSize = sizeof(device);
    bool registered = false;
    const WCHAR ids[] = L"ROOT\\AudioWire\0";
    bool ok = SetupDiCreateDeviceInfoW(set, L"AudioWire", &media, L"AudioWire virtual audio", nullptr,
                                       DICD_GENERATE_ID, &device) &&
              SetupDiSetDeviceRegistryPropertyW(set, &device, SPDRP_HARDWAREID,
                                                reinterpret_cast<const BYTE*>(ids), sizeof(ids)) &&
              SetupDiCallClassInstaller(DIF_REGISTERDEVICE, set, &device);
    registered = ok;
    if (ok)
        ok = UpdateDriverForPlugAndPlayDevicesW(nullptr, L"ROOT\\AudioWire", inf.c_str(), INSTALLFLAG_FORCE,
                                                &reboot) != FALSE;
    DWORD error = ok ? 0 : GetLastError();
    if (!ok && registered)
        SetupDiCallClassInstaller(DIF_REMOVE, set, &device);
    SetupDiDestroyDeviceInfoList(set);
    return error ? static_cast<int>(error) : reboot ? 3010 : 0;
}
static int uninstall() {
    HDEVINFO set = SetupDiGetClassDevsW(&media, nullptr, nullptr, 0);
    if (set == INVALID_HANDLE_VALUE)
        return static_cast<int>(GetLastError());
    int error = 0;
    std::vector<std::wstring> packages;
    for (DWORD i = 0;; ++i) {
        SP_DEVINFO_DATA device{};
        device.cbSize = sizeof(device);
        if (!SetupDiEnumDeviceInfo(set, i, &device))
            break;
        if (!isAudioWire(set, device))
            continue;
        // Determine the exact installed INF of this device, never delete by wildcard.
        WCHAR inf[4096]{};
        DEVPROPTYPE type = 0;
        DWORD needed = 0;
        const DEVPROPKEY infKey = {
            {0xa8b865dd, 0x2e3d, 0x4094, {0xad, 0x97, 0xe5, 0x93, 0xa7, 0x0c, 0x75, 0xd6}}, 5};
        if (SetupDiGetDevicePropertyW(set, &device, &infKey, &type, reinterpret_cast<BYTE*>(inf), sizeof(inf),
                                      &needed, 0))
            packages.emplace_back(inf);
        if (!SetupDiCallClassInstaller(DIF_REMOVE, set, &device))
            error = static_cast<int>(GetLastError());
    }
    SetupDiDestroyDeviceInfoList(set);
    if (!error)
        for (auto& inf : packages) {
            auto slash = inf.find_last_of(L"\\/");
            auto file = slash == std::wstring::npos ? inf : inf.substr(slash + 1);
            if (!SetupUninstallOEMInfW(file.c_str(), 0, nullptr) && GetLastError() != ERROR_FILE_NOT_FOUND)
                error = static_cast<int>(GetLastError());
        }
    return error;
}
static int cleanupStartup() {
    WCHAR path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, 32768))
        return static_cast<int>(GetLastError());
    std::wstring exe = path;
    auto slash = exe.find_last_of(L"\\\\/");
    if (slash == std::wstring::npos)
        return ERROR_INVALID_NAME;
    exe = exe.substr(0, slash + 1) + L"AudioWire.exe";
    int error = 0;
    for (DWORD i = 0;; ++i) {
        WCHAR hive[256]{};
        DWORD length = 256;
        auto rc = RegEnumKeyExW(HKEY_USERS, i, hive, &length, nullptr, nullptr, nullptr, nullptr);
        if (rc == ERROR_NO_MORE_ITEMS)
            break;
        if (rc != ERROR_SUCCESS) {
            error = rc;
            continue;
        }
        std::wstring keyPath =
            std::wstring(hive) + L"\\\\Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Run";
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_USERS, keyPath.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key) !=
            ERROR_SUCCESS)
            continue;
        WCHAR command[32768]{};
        DWORD bytes = sizeof(command), type = 0;
        if (RegQueryValueExW(key, L"AudioWire", nullptr, &type, reinterpret_cast<BYTE*>(command), &bytes) ==
                ERROR_SUCCESS &&
            type == REG_SZ && bytes < sizeof(command)) {
            int count = 0;
            auto args = CommandLineToArgvW(command, &count);
            if (args) {
                if (count && _wcsicmp(args[0], exe.c_str()) == 0) {
                    rc = RegDeleteValueW(key, L"AudioWire");
                    if (rc != ERROR_SUCCESS)
                        error = rc;
                }
                LocalFree(args);
            }
        }
        RegCloseKey(key);
    }
    return error;
}
int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::wcerr << L"AudioWireDriverSetup install <signed-inf> | uninstall\n";
        return 2;
    }
    if (!IsUserAnAdmin()) {
        std::wcerr << L"Administrator access is required to change the AudioWire driver.\n";
        return ERROR_ACCESS_DENIED;
    }
    if (std::wstring(argv[1]) == L"cleanup-startup")
        return cleanupStartup();
    if (std::wstring(argv[1]) == L"uninstall")
        return uninstall();
    if (argc == 3 && std::wstring(argv[1]) == L"install") {
        WCHAR path[32768];
        if (!GetFullPathNameW(argv[2], 32768, path, nullptr))
            return static_cast<int>(GetLastError());
        return install(path);
    }
    return 2;
}
