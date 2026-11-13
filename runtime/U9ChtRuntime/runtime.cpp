#include "platform.h"
#include "version_manifest.h"
#include "engine.h"
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <memory>

namespace {
u9cht::Logger log_file;
bool initialized = false;
// Crash diagnostics (requested 2026-11-13 after the session-38 crash on loading after death). A vectored handler
// logs the first 64 faults (access violation, illegal instruction, divide by zero) with the faulting module and
// offset and the code addresses found on the stack, then lets normal handling continue. It never changes control
// flow. First-chance faults that a __try in the runtime or the game handles are logged too, so the LAST "exception"
// event before an abnormal exit is the crash.
void* crash_handler = nullptr;
std::atomic<uint32_t> crash_events{0};
std::string module_of(uintptr_t address) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(address), &module) || !module) return "?";
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(module, path, MAX_PATH);
    const std::wstring name = std::filesystem::path(path).filename().wstring();
    std::string out;
    for (const auto c : name) out += c < 0x80 ? char(c) : '?';
    char offset[32]; std::snprintf(offset, sizeof offset, "+0x%X", unsigned(address - reinterpret_cast<uintptr_t>(module)));
    return out + offset;
}
LONG CALLBACK crash_logger(EXCEPTION_POINTERS* info) {
    const auto code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_PRIV_INSTRUCTION) return EXCEPTION_CONTINUE_SEARCH;
    if (crash_events.fetch_add(1) >= 64) return EXCEPTION_CONTINUE_SEARCH;
    try {
        const auto* r = info->ExceptionRecord;
        const auto* c = info->ContextRecord;
        const auto at = reinterpret_cast<uintptr_t>(r->ExceptionAddress);
        char head[160];
        std::snprintf(head, sizeof head, "\"code\":%lu,\"address\":%u,\"access\":%u,\"target\":%u,\"thread\":%lu,",
                      static_cast<unsigned long>(code), unsigned(at),
                      r->NumberParameters > 0 ? unsigned(r->ExceptionInformation[0]) : 0u,
                      r->NumberParameters > 1 ? unsigned(r->ExceptionInformation[1]) : 0u, GetCurrentThreadId());
        std::string fields = head;
        fields += "\"module\":\"" + module_of(at) + "\"";
        char regs[200];
        std::snprintf(regs, sizeof regs, ",\"eax\":%lu,\"ebx\":%lu,\"ecx\":%lu,\"edx\":%lu,\"esi\":%lu,\"edi\":%lu,\"ebp\":%lu,\"esp\":%lu",
                      c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
        fields += regs;
        // Code addresses on the stack (u9.exe .text or any module), most recent first; at most 12.
        fields += ",\"stack\":[";
        uint32_t found = 0;
        const auto* sp = reinterpret_cast<const uintptr_t*>(c->Esp);
        for (int i = 0; i < 256 && found < 12; ++i) {
            uintptr_t value = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), sp + i, &value, sizeof value, nullptr)) break;
            const bool game_code = value >= 0x401000 && value < 0x734000;
            if (!game_code) continue;
            if (found++) fields += ",";
            fields += std::to_string(value);
        }
        fields += "]";
        log_file.event("exception", fields);
    } catch (...) {}
    return EXCEPTION_CONTINUE_SEARCH;
}
}

extern "C" DWORD WINAPI U9ChtInitialize(void* argument) noexcept {
    auto* config = static_cast<u9cht::Config*>(argument);
    if (!config || !u9cht::valid_protocol(*config)) return 0;
    config->state = u9cht::State::Initializing;
    config->error = u9cht::Error::Initialization;
    try {
        u9cht::require(!initialized, "runtime already initialized");
        const auto game = u9cht::config_path(config->game_dir);
        log_file.open(u9cht::config_path(config->log_path), false);
        log_file.event("runtime_initializing", "\"version\":\""+std::string(u9cht::RuntimeName)+"\",\"hooks\":0");
        config->process_id = GetCurrentProcessId();
        config->image_base = reinterpret_cast<uint32_t>(GetModuleHandleW(nullptr));
        config->error = u9cht::Error::ImageBase;
        u9cht::require(config->image_base == u9cht::GameBase, "unexpected game image base");
        config->error = u9cht::Error::Baseline;
        u9cht::require(u9cht::known_exe(u9cht::sha256(game / L"u9.exe")), "executable hash mismatch");
        u9cht::require(u9cht::sha256(game / L"ddraw.dll") == u9cht::WrapperHash, "wrapper hash mismatch");
        u9cht::require(std::filesystem::equivalent(u9cht::module_path(nullptr), game / L"u9.exe"),
                       "running executable path mismatch");
        config->error = u9cht::Error::Configuration;
        u9cht::require(!IsDebuggerPresent(), "normal-launch gate requires no attached debugger");
        if (auto wrapper = GetModuleHandleW(L"ddraw.dll")) {
            config->error = u9cht::Error::WrapperBase;
            config->wrapper_base = reinterpret_cast<uint32_t>(wrapper);
            u9cht::require(config->wrapper_base == u9cht::WrapperBase &&
                           std::filesystem::equivalent(u9cht::module_path(wrapper), game / L"ddraw.dll"),
                           "unexpected loaded wrapper");
        }
        config->error = u9cht::Error::UnsupportedMode;
        if (config->mode != u9cht::Mode::Bootstrap) {
            config->error = u9cht::Error::Initialization;
            u9cht::engine = new u9cht::Engine;
            u9cht::engine->install(*config, log_file);
            config->hooks_installed = static_cast<uint32_t>(u9cht::engine->hooks.records().size());
        }
        config->error = u9cht::Error::ForcedFailure;
        u9cht::require(!(config->flags & u9cht::ForceFailure), "requested initialization failure test");
        u9cht::require(log_file.failures() == 0,"initialization diagnostic write failed");
        initialized = true;
        if (!crash_handler) crash_handler = AddVectoredExceptionHandler(0, crash_logger);
        config->state = u9cht::State::Ready; config->error = u9cht::Error::None;
        log_file.event("runtime_ready", "\"hooks\":"+std::to_string(config->hooks_installed)+
                       ",\"debugger_attached\":false,\"debugger_required\":false");
        return 1;
    } catch (...) {
        config->state = u9cht::State::Fail;
        if (u9cht::engine) {
            if (!u9cht::engine->shutdown(true)) config->error = u9cht::Error::Rollback;
            else { delete u9cht::engine; u9cht::engine = nullptr; config->hooks_installed = 0; }
        }
        log_file.event("runtime_fail", "\"error\":" + std::to_string(static_cast<uint32_t>(config->error)));
        initialized = false; log_file.close(); return 0;
    }
}
extern "C" DWORD WINAPI U9ChtShutdown(void*) noexcept {
    if (crash_handler) { RemoveVectoredExceptionHandler(crash_handler); crash_handler = nullptr; }
    if (u9cht::engine) {
        if (!u9cht::engine->shutdown(true)) return 0;
        delete u9cht::engine; u9cht::engine = nullptr;
    }
    log_file.event("runtime_stopped", "\"hooks_restored\":0,\"active_calls\":0");
    initialized = false; log_file.close(); return 1;
}
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    // No initialization, waits, GDI, loader calls or logging under loader lock.
    if (reason == DLL_PROCESS_DETACH) log_file.close();
    return TRUE;
}
