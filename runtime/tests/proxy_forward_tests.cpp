// U9ChtProxy.dll as a plain dsound forwarder (no U9CHT_LAUNCH, so it must do nothing else). No game asset is read.
// Each call goes once through the proxy and once to the system dsound.dll; the results must agree. A machine
// without a sound device gives the same error both ways, which still passes.
#include <windows.h>
#include <unknwn.h>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
int failures = 0;
void check(bool ok, const char* what) { if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; } }

using EnumerateA = HRESULT (WINAPI*)(BOOL (CALLBACK*)(GUID*, LPCSTR, LPCSTR, void*), void*);
using EnumerateW = HRESULT (WINAPI*)(BOOL (CALLBACK*)(GUID*, LPCWSTR, LPCWSTR, void*), void*);
using Create = HRESULT (WINAPI*)(const GUID*, IUnknown**, IUnknown*);
using CanUnload = HRESULT (WINAPI*)();
BOOL CALLBACK count_a(GUID*, LPCSTR, LPCSTR, void* context) { ++*static_cast<int*>(context); return TRUE; }
BOOL CALLBACK count_w(GUID*, LPCWSTR, LPCWSTR, void* context) { ++*static_cast<int*>(context); return TRUE; }

const char* const Names[] = {"DirectSoundCreate", "DirectSoundEnumerateA", "DirectSoundEnumerateW", "DllCanUnloadNow",
    "DllGetClassObject", "DirectSoundCaptureCreate", "DirectSoundCaptureEnumerateA", "DirectSoundCaptureEnumerateW",
    "GetDeviceID", "DirectSoundFullDuplexCreate", "DirectSoundCreate8", "DirectSoundCaptureCreate8"};
}

int wmain() {
    check(GetEnvironmentVariableW(L"U9CHT_LAUNCH", nullptr, 0) == 0, "U9CHT_LAUNCH must not be set for this test");
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    const auto proxy_path = std::filesystem::path(self).parent_path() / L"U9ChtProxy.dll";
    wchar_t system_dir[MAX_PATH]; GetSystemDirectoryW(system_dir, MAX_PATH);
    const HMODULE proxy = LoadLibraryW(proxy_path.c_str());
    const HMODULE system = LoadLibraryW((std::wstring(system_dir) + L"\\dsound.dll").c_str());
    check(proxy && system && proxy != system, "both modules load, as two modules");
    if (!proxy || !system) return 1;

    // The game's entry point is not this test's: the proxy must not have patched anything without the variable.
    for (int ordinal = 1; ordinal <= 12; ++ordinal) {
        const auto by_name = GetProcAddress(proxy, Names[ordinal - 1]);
        const auto by_ordinal = GetProcAddress(proxy, MAKEINTRESOURCEA(ordinal));
        check(by_name && by_name == by_ordinal, "export name and ordinal agree");
        check(GetProcAddress(system, MAKEINTRESOURCEA(ordinal)) == GetProcAddress(system, Names[ordinal - 1]),
              "the system dsound has the same name at that ordinal");
    }

    {   // ordinal 2, the one u9.exe imports for enumeration
        int through = 0, direct = 0;
        const HRESULT a = reinterpret_cast<EnumerateA>(GetProcAddress(proxy, MAKEINTRESOURCEA(2)))(count_a, &through);
        const HRESULT b = reinterpret_cast<EnumerateA>(GetProcAddress(system, "DirectSoundEnumerateA"))(count_a, &direct);
        check(a == b && through == direct, "DirectSoundEnumerateA agrees");
        std::cout << "DirectSoundEnumerateA: hr=" << std::hex << a << std::dec << " devices=" << through << '\n';
        int through_w = 0, direct_w = 0;
        const HRESULT c = reinterpret_cast<EnumerateW>(GetProcAddress(proxy, "DirectSoundEnumerateW"))(count_w, &through_w);
        const HRESULT d = reinterpret_cast<EnumerateW>(GetProcAddress(system, "DirectSoundEnumerateW"))(count_w, &direct_w);
        check(c == d && through_w == direct_w && through_w == through, "DirectSoundEnumerateW agrees");
    }
    {   // ordinal 1, the one u9.exe imports to open the device
        IUnknown* through = nullptr; IUnknown* direct = nullptr;
        const HRESULT a = reinterpret_cast<Create>(GetProcAddress(proxy, MAKEINTRESOURCEA(1)))(nullptr, &through, nullptr);
        const HRESULT b = reinterpret_cast<Create>(GetProcAddress(system, "DirectSoundCreate"))(nullptr, &direct, nullptr);
        check(a == b && (through != nullptr) == (direct != nullptr), "DirectSoundCreate agrees");
        std::cout << "DirectSoundCreate: hr=" << std::hex << a << std::dec << '\n';
        if (through) through->Release();
        if (direct) direct->Release();
        // An invalid argument must come back as the system's own error, with the stack intact.
        const HRESULT e = reinterpret_cast<Create>(GetProcAddress(proxy, "DirectSoundCreate"))(nullptr, nullptr, nullptr);
        const HRESULT f = reinterpret_cast<Create>(GetProcAddress(system, "DirectSoundCreate"))(nullptr, nullptr, nullptr);
        check(e == f && FAILED(e), "an invalid call fails the same way");
    }
    {   // no arguments
        const HRESULT a = reinterpret_cast<CanUnload>(GetProcAddress(proxy, "DllCanUnloadNow"))();
        const HRESULT b = reinterpret_cast<CanUnload>(GetProcAddress(system, "DllCanUnloadNow"))();
        check(a == b, "DllCanUnloadNow agrees");
    }
    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "proxy forward tests passed\n";
    return 0;
}
