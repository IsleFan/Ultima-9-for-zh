#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace u9cht {
inline constexpr uint32_t ProtocolMagic = 0x31484355;
inline constexpr uint32_t ProtocolVersion = 4;
inline constexpr uint32_t RuntimeVersion = 0x00020000;
enum class Mode : uint32_t { Bootstrap, Observe, Render };
enum class State : uint32_t { Empty, Initializing, Ready, Fail, Stopped };
enum class Error : uint32_t {
    None, Protocol, Version, Baseline, ImageBase, WrapperBase, Configuration,
    ForcedFailure, UnsupportedMode, Initialization, Rollback
};
inline constexpr uint32_t ForceFailure = 1;
inline constexpr size_t PathCapacity = 1024;
// Protocol 3: player-selectable enhancements ([features] in the settings INI, the front page's switches).
inline constexpr uint32_t FeatureHoverNames = 1, FeatureWheelPages = 2, FeatureWheelZoom = 4,
                          FeatureMovieSubtitles = 8, FeatureInputProbe = 16, FeatureStatusNumbers = 32,
                          FeatureFloatingNumbers = 64, FeatureTargetBar = 128,
                          FeatureSmartJump = 256,   // the jump key climbs when the game finds something to climb
                          FeatureClickSkip = 512,   // in a conversation, a click anywhere skips the spoken line
                          FeatureEscContents = 1024; // Esc with nothing open opens the journal's table of contents
inline constexpr uint32_t AllFeatures = 2047;
inline constexpr uint32_t DefaultFeatures = FeatureHoverNames | FeatureWheelPages | FeatureWheelZoom | FeatureMovieSubtitles |
                                            FeatureStatusNumbers | FeatureFloatingNumbers |
                                            FeatureTargetBar | FeatureClickSkip | FeatureEscContents;
inline constexpr uint32_t MinHoverDelayMs = 100, MaxHoverDelayMs = 2000, DefaultHoverDelayMs = 250;
// In-game hotkey Ctrl+Shift+K (2026-11-15): the runtime posts OpenKeyBindingsMessage (RegisterWindowMessageW) to the
// front page's window, with the game's window in wParam; the front page opens its key bindings page. The game itself
// binds nothing to Ctrl+Shift+K, and the front page does not let the player bind it.
inline constexpr wchar_t FrontWindowClass[] = L"U9ChtFront";
inline constexpr wchar_t OpenKeyBindingsMessage[] = L"U9ChtFront.OpenKeyBindings";
inline constexpr unsigned OpenKeyBindingsVk = 'K';   // with Ctrl and Shift, no Alt
// Only fixed-width integers and UTF-16 arrays cross the x86 process boundary.
struct Config {
    uint32_t magic = ProtocolMagic;
    uint32_t size = sizeof(Config);
    uint32_t protocol = ProtocolVersion;
    uint32_t runtime_version = RuntimeVersion;
    Mode mode = Mode::Bootstrap;
    uint32_t flags = 0;
    State state = State::Empty;
    Error error = Error::None;
    uint32_t process_id = 0;
    uint32_t image_base = 0;
    uint32_t wrapper_base = 0;
    uint32_t hooks_installed = 0;
    char16_t game_dir[PathCapacity]{};
    char16_t log_path[PathCapacity]{};
    char16_t font_path[PathCapacity]{};
    char16_t allowlist_path[PathCapacity]{};
    // P9-2 player settings ([journal] in the settings INI); validated by the launcher and the runtime.
    uint32_t journal_letter_spacing = 0;
    uint32_t journal_line_spacing = 0;
    uint32_t features = DefaultFeatures;
    uint32_t hover_delay_ms = DefaultHoverDelayMs;
    // Protocol 4: spoken text, subtitles and dialogue choices ([dialogue] in the settings INI). Logical units.
    uint32_t dialogue_letter_spacing = 0;
    uint32_t dialogue_line_spacing = 0;
};
inline constexpr uint32_t MaxJournalLetterSpacing = 8;
inline constexpr uint32_t MaxJournalLineSpacing = 16;
// Dialogue lines are broken when the translation is built: at most 33 cells of 16 units (528) in a raster 595
// units wide (translation/resource-capabilities.json). Two more units per character still fit: 33*18 = 594.
inline constexpr uint32_t MaxDialogueLetterSpacing = 2;
inline constexpr uint32_t MaxDialogueLineSpacing = 8;
static_assert(sizeof(void*) == 4, "runtime ABI is x86 only");
static_assert(sizeof(char16_t) == 2 && sizeof(wchar_t) == 2);
static_assert(std::is_trivially_copyable_v<Config>);
static_assert(sizeof(Config) == 8264);
inline bool valid_protocol(const Config& c) {
    return c.magic == ProtocolMagic && c.size == sizeof(Config) &&
           c.protocol == ProtocolVersion && c.runtime_version == RuntimeVersion &&
           c.mode <= Mode::Render && (c.flags & ~ForceFailure) == 0 &&
           c.journal_letter_spacing <= MaxJournalLetterSpacing && c.journal_line_spacing <= MaxJournalLineSpacing &&
           c.dialogue_letter_spacing <= MaxDialogueLetterSpacing && c.dialogue_line_spacing <= MaxDialogueLineSpacing &&
           (c.features & ~AllFeatures) == 0 && c.hover_delay_ms >= MinHoverDelayMs && c.hover_delay_ms <= MaxHoverDelayMs;
}

// Proxy loading (2026-11-16; docs/research/U9_PROXY_DLL_LOADING.md). U9ChtLauncher --load proxy copies
// U9ChtProxy.dll to <game>\dsound.dll, writes a LaunchFile, and starts u9.exe normally with LaunchVariable naming
// that file. The proxy loads the runtime at the game's entry point and writes a LaunchResult to
// <launch file> + LaunchResultSuffix. Without the variable the proxy only forwards dsound.
inline constexpr wchar_t LaunchVariable[] = L"U9CHT_LAUNCH";
inline constexpr wchar_t LaunchResultSuffix[] = L".result";
inline constexpr wchar_t ProxyInstalledName[] = L"dsound.dll";
inline constexpr uint32_t LaunchMagic = 0x464C3955;   // "U9LF"
// A byte string inside U9ChtProxy.dll: a dsound.dll in the game folder is ours only when it holds this.
#define U9CHT_PROXY_MARKER "U9ChtProxy/dsound-forwarder/3b1d7a52-9c0e-4f6b-8a47-5e2f0c9d1a66"
enum class LaunchStage : uint32_t { LaunchFile, LoadRuntime, Initialize, Ready };
struct LaunchFile {
    uint32_t magic = LaunchMagic;
    uint32_t config_size = sizeof(Config);
    Config config;
    wchar_t runtime_path[PathCapacity]{};
};
struct LaunchResult {
    uint32_t magic = LaunchMagic;
    uint32_t ok = 0;
    LaunchStage stage = LaunchStage::LaunchFile;   // how far the proxy got
    uint32_t rolled_back = 0;                      // after a failed initialize: U9ChtShutdown returned 1
    Config config;                                 // as U9ChtInitialize left it
};
static_assert(std::is_trivially_copyable_v<LaunchFile> && std::is_trivially_copyable_v<LaunchResult>);
}
