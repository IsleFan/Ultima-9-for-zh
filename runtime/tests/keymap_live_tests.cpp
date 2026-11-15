// Live keymap reload, decision part (U9ChtRuntime/keymap_live.h). No game memory, no files.
#include "keymap_live.h"
#include <iostream>

using namespace u9cht::keymap_live;
namespace {
int failures = 0;
void check(bool ok, const char* what) { if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; } }
}

int main() {
    const Stamp start{10, 100, 20, 200};
    {   // nothing changes
        Watcher w; w.start(start);
        check(w.poll(0, start) == Step::Idle && w.poll(5000, start) == Step::Idle && !w.pending(), "unchanged files stay idle");
    }
    {   // one change, applied after it settles
        Watcher w; w.start(start);
        Stamp saved = start; saved.keymap_time = 21; saved.keymap_size = 210;
        check(w.poll(1000, saved) == Step::Wait && w.pending(), "a change waits");
        check(w.poll(1500, saved) == Step::Wait, "still settling");
        check(w.poll(2000, saved) == Step::Apply, "applied after SettleMs");
        w.settle(saved);
        check(w.poll(2500, saved) == Step::Idle && !w.pending(), "idle after settle");
    }
    {   // the front page writes the keymap, then Options.ini: the second write restarts the wait
        Watcher w; w.start(start);
        Stamp keymap_written = start; keymap_written.keymap_time = 21;
        Stamp options_written = keymap_written; options_written.options_time = 11;
        check(w.poll(1000, keymap_written) == Step::Wait, "first write");
        check(w.poll(1800, options_written) == Step::Wait, "second write restarts the wait");
        check(w.poll(2500, options_written) == Step::Wait, "not yet settled since the second write");
        check(w.poll(2800, options_written) == Step::Apply, "applied once both writes settled");
    }
    {   // changed and changed back before it settled: nothing to apply
        Watcher w; w.start(start);
        Stamp other = start; other.keymap_size = 1;
        check(w.poll(1000, other) == Step::Wait, "change");
        check(w.poll(1500, start) == Step::Idle && !w.pending(), "changed back");
    }
    {   // a refused state is not retried until the files change again
        Watcher w; w.start(start);
        Stamp bad = start; bad.keymap_time = 99;
        w.poll(0, bad); check(w.poll(SettleMs, bad) == Step::Apply, "bad file reaches apply");
        w.settle(bad);
        check(w.poll(SettleMs * 3, bad) == Step::Idle, "refused state not retried");
        Stamp fixed = bad; fixed.keymap_time = 100;
        check(w.poll(SettleMs * 4, fixed) == Step::Wait, "a later change is seen");
    }
    {   // the keymap file disappears
        Watcher w; w.start(start);
        Stamp gone = start; gone.keymap_time = 0; gone.keymap_size = 0;
        w.poll(0, gone); check(w.poll(SettleMs, gone) == Step::Apply, "missing file reaches apply (the engine refuses it)");
    }
    {   // the name buffer
        check(name_fits("U9CHT.kmp", 11) && name_fits("default.kmp", 11), "shorter or equal fits");
        check(!name_fits("U9CHT-TEST.kmp", 11), "longer does not fit");
        check(!name_fits("", 11) && !name_fits("..\\x.kmp", 20) && !name_fits("a/b.kmp", 20) && !name_fits("c:x", 20), "not a plain name");
        check(!name_fits("U9CHT.kmp", 0), "unknown current length");
    }
    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "keymap live tests passed\n";
    return 0;
}
