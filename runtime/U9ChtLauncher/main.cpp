#include "front_core.h"
#include "platform.h"
#include "version_manifest.h"
#include <tlhelp32.h>
#include <iostream>
#include <memory>
#include <map>
#include <algorithm>
#include <cwctype>

using namespace u9cht;
namespace {
uintptr_t remote_module(DWORD pid, const std::wstring& name, const std::filesystem::path* expected = nullptr) {
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid));
    require(static_cast<bool>(snapshot), "remote module snapshot failed");
    MODULEENTRY32W entry{}; entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot.get(), &entry)) do {
        if (_wcsicmp(entry.szModule, name.c_str()) == 0) {
            if (expected) require(std::filesystem::equivalent(entry.szExePath, *expected), "remote module path mismatch");
            return reinterpret_cast<uintptr_t>(entry.modBaseAddr);
        }
    } while (Module32NextW(snapshot.get(), &entry));
    return 0;
}
uintptr_t remote_system_export(DWORD pid, const char* name) {
    auto address = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), name);
    require(address != nullptr, "system export unavailable");
    HMODULE owner = nullptr;
    require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(address), &owner), "system export owner unavailable");
    const auto path = std::filesystem::path(module_path(owner));
    auto base = remote_module(pid, path.filename(), &path);
    require(base != 0, "remote system module unavailable");
    return base + reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(owner);
}
// P9-1 player settings (INI). Strict: a section or key this runtime does not implement yet is an error,
// so a setting is never silently ignored. Relative paths resolve against the INI's own directory.
void need(bool ok, const std::string& message) { require(ok, message.c_str()); }
std::vector<std::wstring> ini_list(const wchar_t* section, const std::filesystem::path& file) {
    std::vector<wchar_t> buffer(32768);
    const auto n = section ? GetPrivateProfileSectionW(section, buffer.data(), DWORD(buffer.size()), file.c_str())
                           : GetPrivateProfileSectionNamesW(buffer.data(), DWORD(buffer.size()), file.c_str());
    require(n < buffer.size() - 2, "settings file section too large");
    std::vector<std::wstring> items;
    for (const wchar_t* at = buffer.data(); *at; at += wcslen(at) + 1) items.emplace_back(at);
    return items;
}
std::map<std::wstring, std::wstring> read_settings(const std::filesystem::path& file) {
    static const std::map<std::wstring, std::vector<std::wstring>> known = {
        {L"font", {L"file"}}, {L"journal", {L"letter_spacing", L"line_spacing"}},
        {L"dialogue", {L"letter_spacing", L"line_spacing"}},
        {L"features", {L"hover_names", L"hover_delay_ms", L"wheel_pages", L"wheel_zoom", L"movie_subtitles", L"input_probe", L"status_numbers", L"floating_numbers", L"target_bar", L"smart_jump", L"click_skip"}}};
    need(std::filesystem::is_regular_file(file), "settings file not found");
    std::map<std::wstring, std::wstring> values;
    for (const auto& section : ini_list(nullptr, file)) {
        const auto keys = known.find(section);
        need(keys != known.end(), "settings: unsupported section [" + utf8(section) + "]");
        for (const auto& line : ini_list(section.c_str(), file)) {
            const auto eq = line.find(L'=');
            need(eq != std::wstring::npos, "settings: malformed line in [" + utf8(section) + "]");
            auto key = line.substr(0, eq), value = line.substr(eq + 1);
            while (!key.empty() && iswspace(key.back())) key.pop_back();
            while (!value.empty() && iswspace(value.front())) value.erase(value.begin());
            need(std::find(keys->second.begin(), keys->second.end(), key) != keys->second.end(),
                    "settings: unsupported key " + utf8(section) + "." + utf8(key));
            need(values.emplace(section + L"." + key, value).second, "settings: duplicate key " + utf8(key));
        }
    }
    if (values.count(L"font.file")) {
        std::filesystem::path font(values[L"font.file"]);
        if (font.is_relative()) font = file.parent_path() / font;
        values[L"font.file"] = std::filesystem::canonical(font).wstring();
    }
    return values;
}
DWORD remote_call(HANDLE process, uintptr_t entry, void* parameter) {
    Handle thread(CreateRemoteThread(process, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(entry),
                                      parameter, 0, nullptr));
    require(static_cast<bool>(thread), "bootstrap remote thread failed");
    require(WaitForSingleObject(thread.get(), 30000) == WAIT_OBJECT_0, "bootstrap thread timeout");
    DWORD result = 0; require(GetExitCodeThread(thread.get(), &result), "bootstrap return unavailable");
    return result;
}
void write_remote(HANDLE process, void* address, const void* data, size_t size) {
    SIZE_T actual = 0;
    require(WriteProcessMemory(process, address, data, size, &actual) && actual == size,
            "bootstrap configuration write failed");
}
uintptr_t export_rva(const std::filesystem::path& dll, const char* name) {
    // Map only to resolve an export RVA. No local DLL code or DllMain executes.
    auto module = LoadLibraryExW(dll.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    require(module != nullptr, "runtime export map failed");
    auto entry = GetProcAddress(module, name);
    auto result = entry ? reinterpret_cast<uintptr_t>(entry) - reinterpret_cast<uintptr_t>(module) : 0;
    FreeLibrary(module); require(result != 0, "runtime export missing"); return result;
}
}
// --restore-backup (given by the front page): put the player's original static files back as soon as the game
// ends, with the same receipt-based recover() the front page uses. The front page still runs it again when it
// sees the launcher exit; without a receipt that second call does nothing. This covers a front page that is no
// longer there to do it (2026-11-15: Microsoft Defender quarantined U9Cht.exe while the game was running, and the
// player's files stayed replaced until restored by hand).
void restore_after_game(const Logger& log, const std::filesystem::path& game, const std::filesystem::path& backup) {
    if (backup.empty() || game.empty()) return;
    try {
        const auto result = front::recover(game, backup);
        log.event("restore_after_game", std::string("\"ok\":") + (result.ok ? "true" : "false") +
                  ",\"acted\":" + (result.message.empty() ? "false" : "true"));
        if (!result.ok) std::cerr << "restore failed: " << utf8(result.message) << '\n';
    } catch (const std::exception& error) {
        log.event("restore_after_game", "\"ok\":false,\"exception\":true");
        std::cerr << "restore failed: " << error.what() << '\n';
    }
}

// --load proxy (protocol.h, docs/research/U9_PROXY_DLL_LOADING.md): no suspended process, no remote memory, no remote
// thread. U9ChtProxy.dll goes into the game folder as dsound.dll, the game is started normally, and the proxy loads
// the runtime at the game's entry point. These helpers are the launcher's side of that.
bool read_result(const std::filesystem::path& file, LaunchResult& result) {
    Handle f(CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!f) return false;
    DWORD got = 0;
    return ReadFile(f.get(), &result, sizeof(result), &got, nullptr) && got == sizeof(result) && result.magic == LaunchMagic;
}
// After the game process has ended: the proxy leaves the game folder and the two launch files go.
void cleanup_proxy(const Logger& log, const std::filesystem::path& game, const std::filesystem::path& launch) noexcept {
    if (game.empty()) return;
    try {
        const bool removed = front::remove_proxy(game);
        std::error_code ec;
        std::filesystem::remove(launch, ec);
        std::filesystem::remove(launch.wstring() + LaunchResultSuffix, ec);
        log.event("proxy_removed", std::string("\"ok\":") + (removed ? "true" : "false"));
        if (!removed) std::cerr << "could not remove dsound.dll from the game folder\n";
    } catch (...) { log.event("proxy_removed", "\"ok\":false,\"exception\":true"); }
}

int wmain(int argc, wchar_t** argv) {
    Logger log;
    std::filesystem::path restore_game, restore_backup, proxy_game, launch_path;
    Handle process, primary;
    DWORD pid = 0;
    void* remote_data = nullptr;
    bool resumed = false;
    try {
        std::map<std::wstring, std::wstring> options;
        bool fail = false, validate = false, unload = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring key(argv[i]);
            if (key == L"--force-init-failure") fail = true;
            else if (key == L"--validate-only") validate = true;
            else if (key == L"--verify-unload") unload = true;
            else {
                require(i + 1 < argc && (key == L"--game-dir" || key == L"--runtime" || key == L"--log" ||
                         key == L"--mode" || key == L"--font" || key == L"--allowlist" || key == L"--config" ||
                         key == L"--restore-backup" || key == L"--load" || key == L"--proxy"), "unknown/missing argument");
                require(options.emplace(key, argv[++i]).second, "duplicate option");
            }
        }
        require(options.count(L"--game-dir") && options.count(L"--log"), "--game-dir and fresh --log are required");
        const auto game = std::filesystem::canonical(options[L"--game-dir"]);
        if (options.count(L"--restore-backup")) {
            // Set before anything can fail, so every exit path below restores.
            restore_game = game; restore_backup = std::filesystem::absolute(options[L"--restore-backup"]);
        }
        const auto dll = std::filesystem::canonical(options.count(L"--runtime") ? options[L"--runtime"] :
                         (std::filesystem::path(module_path(nullptr)).parent_path() / L"U9ChtRuntime.dll").wstring());
        const auto log_path = std::filesystem::absolute(options[L"--log"]);
        require(std::filesystem::is_directory(log_path.parent_path()), "log directory must exist");
        require(known_exe(sha256(game / L"u9.exe")) && sha256(game / L"ddraw.dll") == WrapperHash,
                "STOP: executable/wrapper baseline mismatch");
        Config config;
        if (options.count(L"--mode")) {
            auto mode = options[L"--mode"];
            require(mode == L"bootstrap" || mode == L"observe" || mode == L"render", "unsupported mode");
            config.mode = mode == L"bootstrap" ? Mode::Bootstrap : mode == L"observe" ? Mode::Observe : Mode::Render;
        }
        copy_path(config.game_dir, game); copy_path(config.log_path, log_path);
        // Journal defaults (P9-2): a little air around 16 px Chinese on the 256 px page texture.
        config.journal_letter_spacing = 2; config.journal_line_spacing = 3;
        if (options.count(L"--config")) {
            const auto settings = read_settings(std::filesystem::canonical(options[L"--config"]));
            auto number = [&](const wchar_t* key, uint32_t max, uint32_t& out) {
                if (!settings.count(key)) return;
                const auto& text = settings.at(key);
                need(!text.empty() && text.size() <= 2 && std::all_of(text.begin(), text.end(), iswdigit),
                     "settings: " + utf8(key) + " must be a whole number");
                const auto value = uint32_t(std::stoul(text));
                need(value <= max, "settings: " + utf8(key) + " must be 0.." + std::to_string(max));
                out = value;
            };
            auto toggle = [&](const wchar_t* key, uint32_t bit) {
                if (!settings.count(key)) return;
                const auto& text = settings.at(key);
                need(text == L"0" || text == L"1", "settings: " + utf8(key) + " must be 0 or 1");
                config.features = text == L"1" ? (config.features | bit) : (config.features & ~bit);
            };
            toggle(L"features.hover_names", FeatureHoverNames);
            toggle(L"features.wheel_pages", FeatureWheelPages);
            toggle(L"features.wheel_zoom", FeatureWheelZoom);
            toggle(L"features.movie_subtitles", FeatureMovieSubtitles);
            toggle(L"features.input_probe", FeatureInputProbe);
            toggle(L"features.status_numbers", FeatureStatusNumbers);
            toggle(L"features.floating_numbers", FeatureFloatingNumbers);
            toggle(L"features.target_bar", FeatureTargetBar);
            toggle(L"features.smart_jump", FeatureSmartJump);
            toggle(L"features.click_skip", FeatureClickSkip);
            if (settings.count(L"features.hover_delay_ms")) {
                const auto& text = settings.at(L"features.hover_delay_ms");
                need(!text.empty() && text.size() <= 4 && std::all_of(text.begin(), text.end(), iswdigit),
                     "settings: features.hover_delay_ms must be a whole number");
                const auto value = uint32_t(std::stoul(text));
                need(value >= MinHoverDelayMs && value <= MaxHoverDelayMs, "settings: features.hover_delay_ms must be " +
                     std::to_string(MinHoverDelayMs) + ".." + std::to_string(MaxHoverDelayMs));
                config.hover_delay_ms = value;
            }
            number(L"journal.letter_spacing", MaxJournalLetterSpacing, config.journal_letter_spacing);
            number(L"journal.line_spacing", MaxJournalLineSpacing, config.journal_line_spacing);
            number(L"dialogue.letter_spacing", MaxDialogueLetterSpacing, config.dialogue_letter_spacing);
            number(L"dialogue.line_spacing", MaxDialogueLineSpacing, config.dialogue_line_spacing);
            if (settings.count(L"font.file")) {
                require(!options.count(L"--font"), "font given both by --font and by the settings file");
                options[L"--font"] = settings.at(L"font.file");
            }
        }
        if (options.count(L"--font")) copy_path(config.font_path, std::filesystem::canonical(options[L"--font"]));
        if (options.count(L"--allowlist")) copy_path(config.allowlist_path, std::filesystem::canonical(options[L"--allowlist"]));
        config.flags = fail ? ForceFailure : 0;
        const auto init_rva = export_rva(dll, "U9ChtInitialize");
        const auto shutdown_rva = export_rva(dll, "U9ChtShutdown");
        log.open(log_path, true);
        log.event("launcher_validated", "\"runtime_version\":"+std::to_string(RuntimeVersion)+
            ",\"bits\":32,\"debugger\":false,\"runtime_sha256\":\""+utf8(sha256(dll))+"\"");
        if (validate) { log.event("validation_only"); return 0; }
        const bool proxy_load = options.count(L"--load") && options[L"--load"] == L"proxy";
        if (options.count(L"--load")) require(proxy_load || options[L"--load"] == L"inject", "--load must be inject or proxy");
        if (proxy_load) {
            require(!unload, "--verify-unload needs --load inject");
            const auto proxy = std::filesystem::canonical(options.count(L"--proxy") ? options[L"--proxy"] :
                               (std::filesystem::path(module_path(nullptr)).parent_path() / L"U9ChtProxy.dll").wstring());
            const auto dll_path = dll.wstring();
            require(dll_path.size() < PathCapacity, "runtime path too long");
            launch_path = log_path.wstring() + L".launch";
            const auto result_path = std::filesystem::path(launch_path.wstring() + LaunchResultSuffix);
            require(launch_path.wstring().size() < PathCapacity - 32, "launch file path too long");
            std::error_code ec; std::filesystem::remove(result_path, ec);
            auto launch = std::make_unique<LaunchFile>();
            launch->config = config;
            std::copy(dll_path.begin(), dll_path.end(), launch->runtime_path);
            require(front::write_bytes(launch_path, std::string(reinterpret_cast<const char*>(launch.get()), sizeof(LaunchFile))),
                    "launch file write failed");
            const auto installed = front::install_proxy(game, proxy);
            proxy_game = game;   // from here every exit path removes the proxy again
            if (!installed.ok) throw std::runtime_error("proxy install refused: " + utf8(installed.message));
            log.event("proxy_installed", "\"proxy_sha256\":\"" + utf8(sha256(proxy)) + "\"");
            // The variable is inherited by the game and removed from this process again at once.
            require(SetEnvironmentVariableW(LaunchVariable, launch_path.c_str()), "launch variable failed");
            STARTUPINFOW startup{}; startup.cb = sizeof(startup);
            PROCESS_INFORMATION info{};
            const BOOL created = CreateProcessW((game / L"u9.exe").c_str(), nullptr, nullptr, nullptr, FALSE, 0,
                                                nullptr, game.c_str(), &startup, &info);
            SetEnvironmentVariableW(LaunchVariable, nullptr);
            require(created, "game creation failed");
            process.reset(info.hProcess); primary.reset(info.hThread); pid = info.dwProcessId;
            log.event("game_created", "\"game_pid\":" + std::to_string(pid) + ",\"load\":\"proxy\"");
            // The proxy writes its result once U9ChtInitialize has returned (a few seconds: the glyph prewarm).
            auto result = std::make_unique<LaunchResult>();
            bool have = false;
            for (const auto deadline = GetTickCount64() + 120000; !have && GetTickCount64() < deadline;) {
                have = read_result(result_path, *result);
                if (!have && WaitForSingleObject(process.get(), 100) == WAIT_OBJECT_0) { have = read_result(result_path, *result); break; }
            }
            require(have, "proxy handshake: no result (the proxy did not start the runtime)");
            config = result->config;
            if (!result->ok || !valid_protocol(config) || config.state != State::Ready || config.process_id != pid) {
                log.event("handshake_fail", "\"error\":" + std::to_string(static_cast<uint32_t>(config.error)) +
                          ",\"stage\":" + std::to_string(static_cast<uint32_t>(result->stage)) +
                          ",\"rolled_back\":" + std::to_string(result->rolled_back));
                WaitForSingleObject(process.get(), 10000);   // the proxy ends the game itself (exit code 2)
                throw std::runtime_error("runtime initialization reported FAIL; the game was stopped");
            }
            log.event("handshake_ready", "\"game_pid\":" + std::to_string(pid) +
                       ",\"image_base\":" + std::to_string(config.image_base) +
                       ",\"wrapper_base\":" + std::to_string(config.wrapper_base) + ",\"load\":\"proxy\"");
            resumed = true; primary.reset();
            log.event("game_resumed", "\"game_pid\":" + std::to_string(pid) + ",\"load\":\"proxy\"");
            std::cout << "READY game_pid=" << pid << " debugger=false\n" << std::flush;
            require(WaitForSingleObject(process.get(), INFINITE) == WAIT_OBJECT_0, "game process wait failed");
            DWORD code = 0; require(GetExitCodeProcess(process.get(), &code), "game exit status unavailable");
            log.event("game_exited", "\"exit_code\":" + std::to_string(code));
            std::cout << "EXIT game_pid=" << pid << " code=" << code << '\n';
            cleanup_proxy(log, proxy_game, launch_path);
            restore_after_game(log, restore_game, restore_backup);
            return code == 0 ? 0 : 3;
        }
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION info{};
        require(CreateProcessW((game / L"u9.exe").c_str(), nullptr, nullptr, nullptr, FALSE, CREATE_SUSPENDED,
                                nullptr, game.c_str(), &startup, &info), "suspended game creation failed");
        process.reset(info.hProcess); primary.reset(info.hThread); pid = info.dwProcessId;
        log.event("game_created_suspended", "\"game_pid\":" + std::to_string(pid));
        initialize_loader(process.get());
        log.event("loader_initialized_primary_suspended");
        const auto dll_text = dll.wstring();
        const auto data_size = sizeof(Config) + (dll_text.size() + 1) * sizeof(wchar_t);
        remote_data = VirtualAllocEx(process.get(), nullptr, data_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        require(remote_data != nullptr, "bootstrap allocation failed");
        auto remote_path = static_cast<unsigned char*>(remote_data) + sizeof(Config);
        write_remote(process.get(), remote_path, dll_text.c_str(), (dll_text.size() + 1) * sizeof(wchar_t));
        const auto runtime = remote_call(process.get(), remote_system_export(pid, "LoadLibraryW"), remote_path);
        require(runtime && remote_module(pid, dll.filename(), &dll) == runtime, "runtime load/module validation failed");
        write_remote(process.get(), remote_data, &config, sizeof(config));
        auto ready = remote_call(process.get(), runtime + init_rva, remote_data);
        SIZE_T actual = 0;
        require(ReadProcessMemory(process.get(), remote_data, &config, sizeof(config), &actual) && actual == sizeof(config),
                "READY/FAIL handshake read failed");
        if (!ready || !valid_protocol(config) || config.state != State::Ready || config.process_id != pid) {
            log.event("handshake_fail", "\"error\":" + std::to_string(static_cast<uint32_t>(config.error)));
            require(remote_call(process.get(), runtime + shutdown_rva, nullptr) == 1, "initialization rollback failed");
            require(remote_call(process.get(), remote_system_export(pid, "FreeLibrary"), reinterpret_cast<void*>(runtime)),
                    "initialization unload failed");
            require(!remote_module(pid, dll.filename()), "runtime remained loaded after rollback");
            require(VirtualFreeEx(process.get(), remote_data, 0, MEM_RELEASE), "rollback configuration release failed");
            remote_data = nullptr;
            log.event("initialization_rollback_verified", "\"primary_resumed\":false,\"owned_allocation_released\":true");
            throw std::runtime_error("runtime initialization reported FAIL; game was not resumed");
        }
        log.event("handshake_ready", "\"game_pid\":" + std::to_string(pid) +
                   ",\"image_base\":" + std::to_string(config.image_base) +
                   ",\"wrapper_base\":" + std::to_string(config.wrapper_base));
        if (unload) {
            require(config.mode == Mode::Bootstrap, "hot unload is only valid for inert bootstrap");
            require(remote_call(process.get(), runtime + shutdown_rva, nullptr) == 1, "bootstrap shutdown failed");
            require(remote_call(process.get(), remote_system_export(pid, "FreeLibrary"), reinterpret_cast<void*>(runtime)),
                    "bootstrap unload failed");
            require(!remote_module(pid, dll.filename()), "bootstrap unload verification failed");
            log.event("bootstrap_unload_verified");
        }
        require(VirtualFreeEx(process.get(), remote_data, 0, MEM_RELEASE), "bootstrap allocation release failed");
        remote_data = nullptr;
        require(ResumeThread(primary.get()) == 1, "primary resume failed"); resumed = true;
        primary.reset(); log.event("game_resumed", "\"game_pid\":" + std::to_string(pid));
        std::cout << "READY game_pid=" << pid << " debugger=false\n" << std::flush;
        require(WaitForSingleObject(process.get(), INFINITE) == WAIT_OBJECT_0, "game process wait failed");
        DWORD code = 0; require(GetExitCodeProcess(process.get(), &code), "game exit status unavailable");
        log.event("game_exited", "\"exit_code\":" + std::to_string(code));
        std::cout << "EXIT game_pid=" << pid << " code=" << code << '\n';
        restore_after_game(log, restore_game, restore_backup);
        return code == 0 ? 0 : 3;
    } catch (const std::exception& error) {
        // The only process ever terminated here is this launcher's own child.
        if (process && !resumed) { TerminateProcess(process.get(), 2); WaitForSingleObject(process.get(), 10000); }
        log.event("launcher_fail", "\"primary_resumed\":" + std::string(resumed ? "true" : "false"));
        std::cerr << error.what() << '\n';
        // A game this launcher resumed may still be running (a wait failure): restoring under it is not safe.
        if (!resumed || (process && WaitForSingleObject(process.get(), 0) == WAIT_OBJECT_0)) {
            cleanup_proxy(log, proxy_game, launch_path);
            restore_after_game(log, restore_game, restore_backup);
        }
        return 2;
    }
}
