// U9ChtProxy.dll: loads the runtime without process injection (user authorization 2026-11-16;
// docs/research/U9_PROXY_DLL_LOADING.md).
//
// The launcher copies this file to <game>\dsound.dll for one play session. u9.exe imports DSOUND.dll statically,
// and dsound is not a KnownDLL, so Windows loads this copy first. Every one of the 12 dsound exports is forwarded
// to the system dsound.dll.
//
// Without the environment variable U9CHT_LAUNCH this DLL does nothing else: a game started from Steam, GOG or by
// hand runs exactly as the original. With it (set by U9ChtLauncher.exe --load proxy):
//   1. DllMain (loader lock held, so it does almost nothing) replaces the five entry bytes of u9.exe with a jump;
//   2. at the entry point, after every static DLL is loaded and before any game code, the bytes are put back, the
//      runtime is loaded and U9ChtInitialize runs with the Config the launcher prepared in the launch file;
//   3. the outcome is written for the launcher; on failure the runtime is rolled back and the process exits with
//      code 2, because the game's static files are the Chinese ones and must not run without the runtime.
// No C++ runtime state: plain Win32 only.
#include "protocol.h"
#include <windows.h>
#include <cstring>

using namespace u9cht;

extern "C" {
// Ordinal -> system dsound function, filled on first use (never under the loader lock).
void* u9cht_real[13]{};
void __cdecl u9cht_resolve();
void __cdecl u9cht_entry();
unsigned char* u9cht_entry_address = nullptr;
}

// Found in the file by install_proxy/remove_proxy (front_core.cpp), so a copy from any build is recognized as ours.
// Exported so the linker keeps it.
extern "C" __declspec(dllexport) const char U9ChtProxyMarker[] = U9CHT_PROXY_MARKER;

namespace {
const char* const Names[13] = {nullptr,
    "DirectSoundCreate", "DirectSoundEnumerateA", "DirectSoundEnumerateW", "DllCanUnloadNow", "DllGetClassObject",
    "DirectSoundCaptureCreate", "DirectSoundCaptureEnumerateA", "DirectSoundCaptureEnumerateW", "GetDeviceID",
    "DirectSoundFullDuplexCreate", "DirectSoundCreate8", "DirectSoundCaptureCreate8"};
INIT_ONCE resolve_once = INIT_ONCE_STATIC_INIT;
BOOL CALLBACK load_system_dsound(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t path[MAX_PATH + 16];
    const UINT n = GetSystemDirectoryW(path, MAX_PATH);   // SysWOW64 for this 32-bit process
    if (!n || n >= MAX_PATH) return TRUE;
    lstrcatW(path, L"\\dsound.dll");
    const HMODULE system = LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!system) return TRUE;
    for (int i = 1; i <= 12; ++i) u9cht_real[i] = reinterpret_cast<void*>(GetProcAddress(system, Names[i]));
    return TRUE;
}

unsigned char original_entry[5];
wchar_t launch_path[PathCapacity];

bool read_launch(LaunchFile& launch) {
    const HANDLE f = CreateFileW(launch_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD got = 0;
    const bool ok = ReadFile(f, &launch, sizeof(launch), &got, nullptr) && got == sizeof(launch);
    CloseHandle(f);
    return ok && launch.magic == LaunchMagic && launch.config_size == sizeof(Config) && valid_protocol(launch.config) &&
           launch.runtime_path[PathCapacity - 1] == 0 && launch.runtime_path[0] != 0;
}
// Written under a temporary name and then renamed, so the launcher never reads half a result.
void write_result(const LaunchResult& result) {
    wchar_t final_path[PathCapacity + 16], temp_path[PathCapacity + 16];
    lstrcpyW(final_path, launch_path); lstrcatW(final_path, LaunchResultSuffix);
    lstrcpyW(temp_path, final_path); lstrcatW(temp_path, L".tmp");
    const HANDLE f = CreateFileW(temp_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD put = 0;
    const bool ok = WriteFile(f, &result, sizeof(result), &put, nullptr) && put == sizeof(result);
    CloseHandle(f);
    if (ok) MoveFileExW(temp_path, final_path, MOVEFILE_REPLACE_EXISTING);
}

// Static storage: these are 16 KB each, too large for comfort on the stack this early.
LaunchFile launch_file;
LaunchResult launch_result;

void start_runtime() {
    using Call = DWORD (WINAPI*)(void*);
    launch_result.magic = LaunchMagic; launch_result.ok = 0; launch_result.stage = LaunchStage::LaunchFile;
    HMODULE runtime = nullptr;
    Call shutdown = nullptr;
    if (read_launch(launch_file)) {
        launch_result.config = launch_file.config;
        launch_result.stage = LaunchStage::LoadRuntime;
        runtime = LoadLibraryW(launch_file.runtime_path);
        const auto initialize = runtime ? reinterpret_cast<Call>(GetProcAddress(runtime, "U9ChtInitialize")) : nullptr;
        shutdown = runtime ? reinterpret_cast<Call>(GetProcAddress(runtime, "U9ChtShutdown")) : nullptr;
        if (initialize && shutdown) {
            launch_result.stage = LaunchStage::Initialize;
            const DWORD ready = initialize(&launch_result.config);
            const auto& c = launch_result.config;
            if (ready && valid_protocol(c) && c.state == State::Ready && c.process_id == GetCurrentProcessId()) {
                launch_result.ok = 1; launch_result.stage = LaunchStage::Ready;
            }
        }
    }
    if (!launch_result.ok && runtime) {   // the same rollback the launcher does after a failed injection
        launch_result.rolled_back = shutdown && shutdown(nullptr) == 1 ? 1u : 0u;
        FreeLibrary(runtime);
    }
    write_result(launch_result);
    if (!launch_result.ok) ExitProcess(2);
}
}

extern "C" void __cdecl u9cht_resolve() { InitOnceExecuteOnce(&resolve_once, load_system_dsound, nullptr, nullptr); }

// The game's entry point, reached through the jump DllMain wrote. Runs once, on the main thread.
extern "C" void __cdecl u9cht_entry() {
    DWORD old = 0;
    if (VirtualProtect(u9cht_entry_address, sizeof(original_entry), PAGE_EXECUTE_READWRITE, &old)) {
        std::memcpy(u9cht_entry_address, original_entry, sizeof(original_entry));
        VirtualProtect(u9cht_entry_address, sizeof(original_entry), old, &old);
        FlushInstructionCache(GetCurrentProcess(), u9cht_entry_address, sizeof(original_entry));
    } else ExitProcess(2);   // cannot put the entry back: nothing sane can follow
    SetEnvironmentVariableW(LaunchVariable, nullptr);   // not inherited by anything the game starts
    start_runtime();
}

extern "C" __declspec(naked) void u9cht_entry_stub() {
    __asm {
        pushad
        pushfd
        call u9cht_entry
        popfd
        popad
        jmp dword ptr [u9cht_entry_address]
    }
}

// One forwarder per export. The arguments are still on the stack as the caller pushed them; `bytes` is what a
// stdcall return must pop if the system function is missing (E_FAIL).
#define U9CHT_PROXY(ordinal, name, bytes)                 \
    extern "C" __declspec(naked) void proxy_##name() {    \
        __asm { call u9cht_resolve }                      \
        __asm { mov eax, dword ptr [u9cht_real + ordinal * 4] } \
        __asm { test eax, eax }                           \
        __asm { jz missing }                              \
        __asm { jmp eax }                                 \
        __asm { missing: mov eax, 0x80004005 }            \
        __asm { ret bytes }                               \
    }
U9CHT_PROXY(1, DirectSoundCreate, 12)
U9CHT_PROXY(2, DirectSoundEnumerateA, 8)
U9CHT_PROXY(3, DirectSoundEnumerateW, 8)
U9CHT_PROXY(4, DllCanUnloadNow, 0)
U9CHT_PROXY(5, DllGetClassObject, 12)
U9CHT_PROXY(6, DirectSoundCaptureCreate, 12)
U9CHT_PROXY(7, DirectSoundCaptureEnumerateA, 8)
U9CHT_PROXY(8, DirectSoundCaptureEnumerateW, 8)
U9CHT_PROXY(9, GetDeviceID, 8)
U9CHT_PROXY(10, DirectSoundFullDuplexCreate, 40)
U9CHT_PROXY(11, DirectSoundCreate8, 12)
U9CHT_PROXY(12, DirectSoundCaptureCreate8, 12)

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(self);
    const DWORD n = GetEnvironmentVariableW(LaunchVariable, launch_path, PathCapacity - 16);
    if (!n || n >= PathCapacity - 16) return TRUE;   // not started by the launcher: forward dsound, nothing else
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    unsigned char* entry = base + nt->OptionalHeader.AddressOfEntryPoint;
    // push ebp; mov ebp,esp; push -1: five whole bytes at u9.exe's entry (VA 0x714920). Anything else is not the
    // executable this was made for; it is left alone and the launcher stops the game when no result arrives.
    static const unsigned char expected[5] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF};
    if (std::memcmp(entry, expected, sizeof(expected)) != 0) return TRUE;
    DWORD old = 0;
    if (!VirtualProtect(entry, sizeof(expected), PAGE_EXECUTE_READWRITE, &old)) return TRUE;
    std::memcpy(original_entry, entry, sizeof(original_entry));
    const auto target = reinterpret_cast<unsigned char*>(&u9cht_entry_stub);
    const int32_t offset = static_cast<int32_t>(target - (entry + 5));
    entry[0] = 0xE9; std::memcpy(entry + 1, &offset, sizeof(offset));
    VirtualProtect(entry, sizeof(expected), old, &old);
    FlushInstructionCache(GetCurrentProcess(), entry, sizeof(expected));
    u9cht_entry_address = entry;
    return TRUE;
}
