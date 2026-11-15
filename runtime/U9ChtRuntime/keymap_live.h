#pragma once
// Live keymap reload (user authorization 2026-11-15; docs/research/U9_EXTENDED_SETUP_TOOL.md, "Changing bindings
// while the game runs"). The game reads its keymap once, at start. When the player saves new bindings on the
// front page while the game runs, the runtime applies them on the game thread with the game's own routines:
// every command's key list is emptied, the built-in bindings are added back, and the keymap reader 0x4D25E0 runs
// again. This header holds the decision part only (no game memory), tested by tests/keymap_live_tests.cpp.
#include <cstdint>
#include <string_view>

namespace u9cht::keymap_live {
inline constexpr uint64_t PollMs = 500;     // how often the two files are looked at
inline constexpr uint64_t SettleMs = 1000;  // a change must stay unchanged this long before it is applied

// Last-write time and size of Options.ini and of the keymap it names.
struct Stamp {
    uint64_t options_time = 0, options_size = 0, keymap_time = 0, keymap_size = 0;
    bool operator==(const Stamp&) const = default;
};

enum class Step { Idle, Wait, Apply };

// Debounces file changes: the front page writes the keymap and then Options.ini, and an editor may save twice.
class Watcher {
public:
    void start(const Stamp& s) { applied_ = seen_ = s; pending_ = false; }
    Step poll(uint64_t now, const Stamp& current) {
        if (!pending_) {
            if (current == applied_) return Step::Idle;
            pending_ = true; seen_ = current; changed_at_ = now; return Step::Wait;
        }
        if (!(current == seen_)) { seen_ = current; changed_at_ = now; }
        if (current == applied_) { pending_ = false; return Step::Idle; }   // changed back before it settled
        return now - changed_at_ >= SettleMs ? Step::Apply : Step::Wait;
    }
    // After an apply, or a refusal: either way this state is not looked at again until the files change.
    void settle(const Stamp& s) { applied_ = seen_ = s; pending_ = false; }
    bool pending() const { return pending_; }
private:
    Stamp applied_, seen_;
    bool pending_ = false;
    uint64_t changed_at_ = 0;
};

// The keymap name goes into the game's own buffer (keymap object +0x14, filled from Options.ini at start). Its
// size is unknown, so a new name may not be longer than the one already there.
inline bool name_fits(std::string_view name, size_t current_length) {
    return !name.empty() && name.size() <= current_length && name.size() < 64 &&
           name.find_first_of("/\\:") == std::string_view::npos && name != "." && name != "..";
}
}
