#pragma once
#include "hook_manager.h"
#include "identity.h"
#include "platform.h"
#include "compositor.h"
#include "raster_runs.h"
#include "misc_marker.h"
#include "clock_text.h"
#include "movie_graph.h"
#include "cheat_notice.h"
#include <atomic>

namespace u9cht {
struct EntryStats {
    uint64_t wide = 0, cache = 0, copies = 0, layout = 0, bounds = 0, glyph = 0;
    uint64_t raster = 0, queue = 0, submissions = 0, presentations = 0, freed = 0;
    uint64_t width_overrides = 0, unicode_draws = 0, composites = 0, cache_hits = 0, cache_misses = 0;
    uint64_t raster_retired = 0, queue_retired = 0;
    uint64_t shared_rasters = 0;
    uint64_t name_copies = 0;
};
struct Placement { uint32_t offset = 0; int32_t x = 0, y = 0; uint32_t run = 0; std::array<uint8_t,3> rgb{181,132,239}; };
// One native font's displayed color, learned once by drawing a reference glyph privately.
struct FontStyle {
    uintptr_t blob = 0;
    std::array<uint8_t,3> rgb{181,132,239};
    bool sampled = false;
    uint32_t reference = 0;
    uint64_t uses = 0;
};
struct RasterBinding {
    uintptr_t address = 0, pixels = 0;
    uint32_t plan = 0, width = 0, height = 0, stride = 0, count = 0;
    uint64_t generation = 0, hash = 0, paint_generation = 0, identity = 0;
    std::array<Placement, MaxText> placements{};
    RasterRuns runs;
};
struct QueueBinding {
    uintptr_t address = 0, raster = 0;
    uint32_t plan = 0;
    uint64_t generation = 0, raster_generation = 0, last_frame = 0, identity = 0;
};
enum class Life : uint32_t { Cache = 1, Copy, BufferEnd, RasterBegin, RasterReady, QueueBegin, QueueEnd, RasterEnd, Shutdown, RasterRunReady };
struct LifeEvent {
    uint64_t sequence = 0, tick = 0, identity = 0, generation = 0;
    uintptr_t address = 0, related = 0;
    uint32_t plan = 0;
    Life kind = Life::Cache;
    uint64_t raster_hash = 0;
    uint32_t width = 0, height = 0, stride = 0;
    uint32_t glyph_count = 0;
};
struct RasterFault {
    uint64_t tick = 0, frame = 0, paint = 0, identity = 0;
    uintptr_t source = 0, raster = 0, paint_owner = 0, widget = 0, paint_caller = 0;
    uint32_t plan = 0, offset = 0, native_width = 0, native_height = 0;
    bool bound = false;
    uint32_t prior_plan = 0, prior_count = 0;
    uint64_t prior_identity = 0, prior_generation = 0, prior_paint = 0;
};
// P8-2 journal probe: observe-only evidence, never changes game output.
struct JournalAppend {
    uint64_t tick = 0;
    int32_t entry = -1;          // matched plan entry, -1 when not allowlisted
    uint32_t code_page = 0, wide_length = 0, capacity = 0, high_bytes = 0;
    int32_t result = 0;
    uint32_t error = 0;
    bool marker = false;         // P8-3 J2: a U9J<entry> marker was written instead
};
// P8-3: the native glyph and offset whose one solid ink pixel is copied, pixel by pixel, onto page rasters.
struct InkProbe {
    uintptr_t blob = 0, glyph = 0;
    uint32_t type = 0, reference = 0, value = 0, solid = 0, written = 0;
    int32_t dx = 0, dy = 0;
    bool usable = false;
    // P10-1: outlined fonts (screen messages) fill with a light shade inside a dark outline. The fill is the ink;
    // the darker outline shade becomes a one-pixel drop shadow drawn first.
    bool shadow = false;
    uint32_t shadow_value = 0;
    int32_t shadow_dx = 0, shadow_dy = 0;
    // P10-1 diagnostics: the reference glyph's written pixels, as (main on black, main on white, aux on 0, aux on ~0).
    struct Sample { uint16_t a = 0, b = 0; uint32_t aux_a = 0, aux_b = 0, count = 0; };
    std::array<Sample,8> samples{};
    uint32_t sample_count = 0, sample_overflow = 0, aux_only = 0, aux_enclosure = 0;
    bool aux_fill = false; // ink taken from an aux-only (main plane untouched) fill pixel
    // P10-1: outlined font (screen messages): a light core inside a thick dark outline. The core shade is the
    // ink and the dark shade is drawn first at all 8 one-pixel offsets.
    bool outline = false;
    uint32_t core_light = 0, dark_pixels = 0;
};
// P9-2: one contiguous run of plan slots inside an expanded diary buffer. Authored line breaks are dropped
// on expansion (English journal sources contain none; the native flow wraps to the page), so one entry
// can yield several segments. Segments are sorted by buffer offset.
struct JournalSegment { uint32_t offset = 0, plan = 0, plan_offset = 0, length = 0; };
struct JournalBuffer {
    uintptr_t base = 0;
    uint32_t size = 0, entries = 0;
    bool misc = false; // P10-1: stripped misctext UI text; its CJK em follows the native font height
    std::vector<JournalSegment> segments;
};
struct FlowProbe {
    uintptr_t flow = 0, caller = 0, widget = 0, text = 0;
    uintptr_t buffers[3]{};      // flow+0x34 (4 KiB), +0x38 (64 KiB), +0x3C (32 KiB)
    uint32_t text_length = 0, text_high = 0;
    uint64_t text_hash = 0, calls = 0, last_tick = 0;
    uint64_t layout[4]{}, bounds[4]{}, glyph[4]{}; // by range: text, +0x34, +0x38, +0x3C
    uintptr_t glyph_paint_caller = 0, glyph_paint_owner = 0, glyph_paint_widget = 0, glyph_raster = 0;
    uint32_t glyph_raster_width = 0, glyph_raster_height = 0;
    uint64_t glyph_outside_paint = 0;
};
struct CallerCount { uintptr_t caller = 0; uint64_t count = 0; };
struct Engine {
    Pack pack;
    Registry registry;
    HookManager hooks;
    Logger* log = nullptr;
    bool render = false;
    SRWLOCK mutex = SRWLOCK_INIT;
    SRWLOCK io_gate = SRWLOCK_INIT;
    std::atomic<bool> accepting{false}, entered{false};
    std::atomic<uint32_t> active{0}, errors{0}, first_error{0};
    Handle worker, stop;
    GlyphCache glyphs;
    FrameList draw_list;
    SurfaceTracker surface;
    std::array<FontStyle,64> fonts{};
    uint32_t font_count = 0;
    uint64_t font_overflow = 0;
    static constexpr uint32_t SampleSize = 128;
    std::array<uint16_t,SampleSize*SampleSize> sample{};
    std::array<uint16_t,SampleSize*SampleSize*2> sample_aux{};
    std::array<uint16_t,SampleSize*SampleSize*2> probe_aux_b{}; // P10-1: aux plane of the white-background pass
    uint64_t occluder_masks = 0;
    std::array<uint64_t,6> occluder_rejects{}; // masks full, tiles, raster fields, size, fault, mostly opaque
    std::array<uint64_t,5> skip_reasons{};     // not current, device lost, backbuffer, stale source, empty
    Rect last_occluder{};
    uint32_t last_occluder_caller = 0, last_occluder_opaque = 0, last_occluder_rule = 0, last_occluder_corner = 0;
    uint32_t last_occluder_type = 0, last_occluder_alpha1555 = 0, last_occluder_alpha4444 = 0;
    std::array<uint32_t,5> last_occluder_raster{}; // tiles, width, height, bits, stride
    uint64_t started = 0, frame = 1, generations = 0, flips = 0;
    uint32_t max_rasters = 0, max_queues = 0;
    uint64_t composite_count = 0, unicode_runs = 0, pixel_writes = 0, lock_failures = 0, unlock_failures = 0;
    uint64_t skipped_frames = 0, compositor_ticks = 0, compositor_max_ticks = 0;
    uint32_t max_frame_records = 0;
    std::atomic<DWORD> render_thread{0};
    std::array<LifeEvent,16384> life_events{}; // Peak observed: 2560 in one 5s snapshot.
    uint32_t life_count = 0, life_dropped = 0;
    uint64_t life_sequence = 0;
    std::array<RasterFault,16> raster_faults{};
    uint32_t raster_fault_count = 0;
    std::array<EntryStats, MaxPlans> stats{};
    std::array<RasterBinding, 64> rasters{};
    std::array<QueueBinding, 128> queues{};
    std::array<JournalAppend,16> journal_appends{};
    uint64_t journal_append_total = 0;
    std::array<FlowProbe,32> flows{};
    uint64_t flow_overflow = 0;
    std::array<CallerCount,32> glyph_callers{}, paint_callers{}, queue_callers{};
    std::array<CallerCount,32> misc_copy_callers{};   // strdup 0x714CD0 callers given a marked misctext
    uint64_t misc_label_binds = 0;
    uintptr_t copy_misc_label(const char* text, uintptr_t caller, uintptr_t (__cdecl* copy)(const char*)) noexcept;
    uint64_t caller_overflow = 0;
    bool texture_enabled = false;
    std::array<InkProbe,16> inks{};
    uint32_t ink_count = 0;
    std::array<uint16_t,SampleSize*SampleSize> probe_a{}, probe_b{};
    // P10-1: UI pages keep several flows alive at once (journal, books and misctext buffers share this ring).
    std::array<JournalBuffer,64> journal_buffers{};
    // P9-4 books: the plan converted at 0x42E64A, consumed by the next flow initializer.
    int pending_book = -1;
    uint64_t book_converts = 0, book_short = 0, book_bound = 0, book_mismatch = 0, book_width_overrides = 0;
    // `locked`: the caller already holds `mutex` (dispatch stages). Both never call into the game.
    bool bind_book(uintptr_t text, uint32_t length, uint32_t plan, bool locked = false) noexcept;
    // P10-1: output buffer of the last book conversion (0x42E64A); signs pass it to the message holder.
    uintptr_t pending_book_buffer = 0;
    uint64_t book_messages = 0, misc_message_strips = 0, shop_converts = 0, shop_short = 0;
    // P10-2 clock: one-character plan per ClockScalars entry (-1 when the pack lacks it; then clock is off).
    std::array<int32_t,sizeof(ClockScalars)/sizeof(char16_t)-1> clock_plans{};
    bool clock_enabled = false;
    uint64_t clock_rewrites = 0, clock_unmatched = 0, clock_too_long = 0;
    bool rewrite_clock(uintptr_t dest, uint32_t length) noexcept; // caller holds `mutex`
    // P11: movies play through an own FilterGraph (CoCreateInstance IAT 0x740418) when movies.tsv is present.
    MovieGraphConfig movie_config;
    bool movies_enabled = false;
    bool input_hooks_enabled = false;  // input probe / wheel pages / hover names (see input_hooks.h)
    uint32_t movie_cue_rejects = 0;
    uint32_t journal_next = 0;
    uint64_t journal_markers = 0, journal_marker_short = 0, journal_expansions = 0, journal_spans = 0, journal_day_headers = 0;
    uint64_t journal_width_overrides = 0, journal_breaks_dropped = 0, journal_spaces_kept = 0, journal_leading_applied = 0;
    uint32_t journal_letter = 0, journal_line = 0;
    uint32_t journal_advance(uint8_t previous, uint32_t base, uint32_t& lead) const noexcept;
    uint64_t journal_unknown_markers = 0, journal_stale = 0, journal_alloc_failures = 0;
    uint64_t texture_glyphs = 0, texture_pixels = 0, texture_faults = 0;
    std::array<uint64_t,4> texture_rejects{}; // slot byte, metric, glyph cache, ink
    const InkProbe* ink_for(uintptr_t record, uintptr_t raster) noexcept;
    int plan_for_entry(uint32_t entry) const noexcept;
    int journal_locate(uintptr_t source, uint32_t& offset, uint8_t* previous = nullptr, bool* misc = nullptr) noexcept;
    bool small_ems = false; // 12/14 px texture glyphs warmed and consistent
    uint64_t small_em_glyphs = 0;
    // CJK em for a font record: TextureEm unless `misc` text and the font is small.
    uint32_t texture_em(uintptr_t record, bool misc) const noexcept;
    bool expand_journal(uintptr_t text_ref, uintptr_t text, uint32_t length) noexcept;
    // P10-1 M1: misctext UI marker bridge (see misc_marker.h).
    uint64_t misc_marked = 0, misc_mark_failures = 0, misc_stripped = 0, misc_buffers = 0;
    uint64_t misc_invalid = 0, misc_partial = 0, misc_leaks = 0, misc_draw_strips = 0, misc_bounded = 0;
    uint64_t name_count_labels = 0; // "(count) name" stack labels bound after the count prefix
    uint64_t book_overwritten = 0; // P10-1: a book conversion replaced a pending book not yet bound
    uint64_t misc_measures = 0, misc_measure_faults = 0;
    // P10-1 diagnostics: unbound 0x7F glyphs inside a recorded flow text, and fonts used by texture glyphs.
    struct FlowMiss { uintptr_t caller = 0, text = 0, buffer = 0; uint32_t offset = 0, buffer_size = 0, segments = 0, first_segment = 0; uint64_t count = 0, slot_misses = 0; std::array<uint8_t,8> bytes{}; uint32_t byte_count = 0; };
    std::array<FlowMiss,16> flow_misses{};
    struct TextureFont { uintptr_t blob = 0; uint32_t height = 0, spacing = 0, extra = 0; uint64_t glyphs = 0; };
    std::array<TextureFont,16> texture_fonts{};
    int32_t measure_misc(uintptr_t record, uintptr_t text, uint32_t length, int32_t native, void* original) noexcept;
    bool mark_misc(uintptr_t object, uint32_t plan) noexcept;
    bool strip_misc(uintptr_t text, uint32_t length, bool locked = false) noexcept;
    uint64_t message_insets = 0;
    void count_caller(std::array<CallerCount,32>& table, uintptr_t caller) noexcept;
    void probe_flow_source(uintptr_t source, uint32_t stage) noexcept;
    bool begin() noexcept;
    void fail(uint32_t code) noexcept;
    void install(const Config& config, Logger& logger);
    bool shutdown(bool explicit_unload) noexcept;
    void snapshot(const char* event) noexcept;
    void flush_movie_trace() noexcept;
    Located locate(uintptr_t source) noexcept;
    RasterBinding* raster(uintptr_t address) noexcept;
    QueueBinding* queue(uintptr_t address) noexcept;
    void retire(uintptr_t address) noexcept;
    void retire_buffer(uintptr_t address) noexcept;
    Binding* bind_buffer(uintptr_t address, uint32_t length, uint32_t plan, uint64_t identity, Life kind, uintptr_t related) noexcept;
    LifeEvent* trace(Life kind, uint32_t plan, uintptr_t address, uint64_t identity, uint64_t generation, uintptr_t related = 0) noexcept;
    void compose(uintptr_t backend) noexcept;
    // Developer cheat notices (cheat_notice.h): bindings of the active keymap, the press waiting for the game to
    // act on it, and the line shown at the top of the screen until `notice_until`.
    std::vector<cheat::Binding> cheat_bindings;
    struct PendingCheat { int binding = -1; uint64_t tick = 0; int before = -1; } pending_cheat;
    std::wstring notice;
    uint64_t notice_until = 0, cheat_presses = 0, cheat_notices = 0, notice_frames = 0, notice_glyph_misses = 0;
    std::vector<uint32_t> unwarmed;   // notice characters the font could not warm (logged at start)
    void on_cheat_key(UINT vk, UINT mods) noexcept;    // game thread, from input_hooks
    void on_spell_cast(uint32_t caster, uint32_t avatar, uint32_t spell) noexcept;   // caster/avatar: objects       // game thread, the Avatar's casts (cast_hook)
    uint64_t spell_casts = 0; uint32_t last_spell = 0;
    struct CastRecord { uint32_t caster = 0, avatar = 0, spell = 0; };
    std::array<CastRecord,8> recent_casts{}; uint64_t cast_calls = 0;   // every caster, newest last
    void show_notice(std::wstring text) noexcept;      // caller holds `mutex`
    void update_notice(uintptr_t backend) noexcept;    // caller holds `mutex`; flip, before compose
    // Overlay text (notices, status numbers): centred on center_x, black outline under `fill`. False when the frame's
    // command list cannot take it (wrong frame/backend or full). Caller holds `mutex`.
    bool overlay_text(uintptr_t backend, std::wstring_view text, int32_t center_x, int32_t baseline, uint32_t em,
                      std::array<uint8_t,3> fill, uint64_t& misses) noexcept;
    // Status numbers on the health and mana bars ("35/98"), requested 2026-11-13 (FeatureStatusNumbers).
    bool status_numbers = false;
    uint64_t status_frames = 0, status_misses = 0, status_unreadable = 0;
    uint32_t status_last[5]{};   // hp, max hp, mana, max mana (+0x3C), mana cap (+0x3E)
    void update_status(uintptr_t backend) noexcept;    // caller holds `mutex`; flip, before compose
    // Floating numbers (FeatureFloatingNumbers, 2026-11-13): HP lost (-N), HP healed by 3 or more (+N) and mana spent
    // (-N) rise from the bars. Values are polled each frame; a jump with no readable frame before it, or a changed
    // maximum (load, level up), only resynchronises.
    bool floating_numbers = false, status_synced = false;
    uint32_t status_previous[5]{};
    struct Floater { std::wstring text; uint64_t start = 0; int32_t x = 0, y = 0, rise = 70; std::array<uint8_t,3> rgb{}; };
    std::array<Floater,16> floaters{};
    uint32_t floater_next = 0;
    uint64_t floaters_spawned = 0, floater_frames = 0, floater_misses = 0;
    void spawn_floater(std::wstring text, int32_t x_units, std::array<uint8_t,3> rgb, int32_t y_units = 0,
                       int32_t rise_units = 70, bool rightward = false) noexcept;
    // Target bar (FeatureTargetBar, 2026-11-13): the combat target's hp/max in a frame at the top centre. Solid
    // rectangles are runtime-owned glyphs (alpha only), index [scale-1]: shade, trough, fill; the fill is clipped to hp.
    bool target_bar = false;
    std::array<std::array<Glyph,3>,2> bar_glyphs{};
    uint64_t target_bar_frames = 0;
    void build_bar_glyphs() noexcept;
    // Combatants (2026-11-14): every frame the 512 stats records ([0x98F31C], 0x27800 bytes, allocated at 0x4F1F68 and
    // re-allocated on load at 0x4F2270) are compared with the previous frame. A record other than the Avatar's whose HP
    // dropped (same max) was hit: it floats its damage and joins the combatant list for CombatantMs. The locked target
    // is always first. Up to MaxBars bars are stacked under the top bars. A changed table address, an unreadable frame or
    // more than ResyncChanges records changing at once (a load) only resynchronises.
    struct Combatant { uint32_t record = 0xFFFFFFFF; uint64_t last_hit = 0, zero_since = 0; int32_t row = 0; };
    std::array<Combatant,8> combatants{};
    std::array<uint16_t,512> record_hp{}, record_max{};
    std::array<uint32_t,512> record_object{};
    uintptr_t record_base = 0;
    bool records_synced = false;
    uint64_t combat_hits = 0, combat_resyncs = 0;
    void scan_combatants(uint32_t avatar_record, uint32_t locked) noexcept;
    void draw_combat(uintptr_t backend, uint32_t scale, uint32_t avatar_record) noexcept;
    void floater_for(uint32_t record, uint32_t damage) noexcept;
    // Target probe (research for the target health bar, 2026-11-13; read-only). Combat manager [0x98F324]:
    // +0x1008 target handle (-1 none), +0x100C lock. Distinct targets seen: handle, lock, object type (+0xA), and the
    // stats record values hp/max (+0x34/+0x36) when the type is a record index (< 0x200), else 0xFFFF.
    struct TargetSeen { uint32_t handle = 0, lock = 0, type = 0, hp = 0, max = 0, min_hp = 0xFFFF; uint64_t frames = 0; };
    std::array<TargetSeen,8> targets_seen{};
    uint64_t target_frames = 0, target_overflow = 0, target_misses = 0, target_lock_frames = 0;
    uint32_t last_target_raw = 0;
    void probe_target() noexcept;
    void add_occluder(uintptr_t node, Rect area, uint32_t caller, float sx, float sy) noexcept;
    const FontStyle& font_style(uintptr_t record) noexcept;
};
extern Engine* engine;
}
