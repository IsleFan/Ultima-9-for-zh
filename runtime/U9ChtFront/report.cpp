#include "report.h"
#include <array>
#include <cstdint>
#include <fstream>

namespace u9cht::front::report {
namespace {
bool has(const std::string& line, const char* needle) { return line.find(needle) != std::string::npos; }

// The end of a JSON string whose first character is at `at` (just after the opening quote): the closing quote.
size_t string_end(const std::string& s, size_t at) {
    for (size_t i = at; i < s.size(); ++i) {
        if (s[i] == '\\') { ++i; continue; }
        if (s[i] == '"') return i;
    }
    return std::string::npos;
}
std::string unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 >= s.size()) { out += s[i]; continue; }
        const char c = s[++i];
        if (c == 'u' && i + 4 < s.size()) {
            const unsigned v = unsigned(std::stoul(s.substr(i + 1, 4), nullptr, 16));
            if (v < 0x80) out += char(v);
            i += 4;
        } else if (c == 'n') out += '\n';
        else if (c == 't') out += '\t';
        else out += c;
    }
    return out;
}
// The value of "name":"..." in `line`, unescaped, or "".
std::string field(const std::string& line, const char* name, size_t from = 0, size_t* next = nullptr) {
    const std::string key = std::string("\"") + name + "\":\"";
    const auto at = line.find(key, from);
    if (at == std::string::npos) return {};
    const auto start = at + key.size(), end = string_end(line, start);
    if (end == std::string::npos) return {};
    if (next) *next = end + 1;
    return unescape(line.substr(start, end - start));
}
void empty_field(std::string& line, const char* name) {
    const std::string key = std::string("\"") + name + "\":\"";
    for (size_t at = line.find(key); at != std::string::npos; at = line.find(key, at + key.size())) {
        const auto start = at + key.size(), end = string_end(line, start);
        if (end == std::string::npos) return;
        line.erase(start, end - start);
    }
}
// Arrays and objects longer than `limit` characters (the per-plan statistics of runtime_counters) become
// "<omitted>"; the line stays valid JSON.
void omit_big_values(std::string& line, size_t limit) {
    for (size_t i = 1; i < line.size(); ++i) {
        if (line[i] == '"') { const auto e = string_end(line, i + 1); if (e == std::string::npos) return; i = e; continue; }
        if ((line[i] != '[' && line[i] != '{') || line[i - 1] != ':') continue;
        int depth = 0; size_t j = i;
        for (; j < line.size(); ++j) {
            if (line[j] == '"') { const auto e = string_end(line, j + 1); if (e == std::string::npos) return; j = e; continue; }
            if (line[j] == '[' || line[j] == '{') ++depth;
            else if ((line[j] == ']' || line[j] == '}') && --depth == 0) break;
        }
        if (j >= line.size()) return;
        if (j - i + 1 > limit) {
            static constexpr char omitted[] = "\"<omitted>\"";
            line.replace(i, j - i + 1, omitted);
            i += sizeof(omitted) - 2;   // on its closing quote; the loop steps past it
        }
    }
}
template <class F> void each_line(const std::string& text, F f) {
    size_t at = 0;
    while (at < text.size()) {
        auto end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        f(line);
        at = end + 1;
    }
}
}

Session summarize(const std::string& jsonl) {
    Session s;
    each_line(jsonl, [&](const std::string& line) {
        if (has(line, "\"event\":\"exception\"")) {
            s.crashed = true;
            if (s.where.empty()) s.where = field(line, "module");
        } else if (has(line, "\"event\":\"game_exited\"")) {
            const auto at = line.find("\"exit_code\":");
            if (at != std::string::npos) {
                try { s.exit_code = std::stoll(line.substr(at + 12)); } catch (...) {}
                if (s.exit_code != 0) s.crashed = true;
            }
        } else if (has(line, "\"event\":\"translation_flag\"")) {
            ++s.flag_presses;
            size_t from = 0, next = 0;
            while (true) {
                const auto key = field(line, "key", from, &next);
                if (key.empty()) break;
                const auto text = field(line, "text", next, &next);
                from = next;
                bool known = false;
                for (const auto& f : s.flags) if (f.key == key) known = true;
                if (!known && s.flags.size() < 500) s.flags.push_back({key, text});
            }
        }
    });
    return s;
}

std::string trimmed_log(const std::string& jsonl) {
    std::string out;
    each_line(jsonl, [&](std::string line) {
        if (line.empty() || has(line, "\"event\":\"glyph_cache_warm\"")) return;
        if (has(line, "\"event\":\"flow_text\"") || has(line, "\"event\":\"book_flow\"")) empty_field(line, "head");
        if (has(line, "\"event\":\"file_probe\"")) empty_field(line, "name");
        if (line.size() > 1000) omit_big_values(line, 1000);
        out += line; out += "\r\n";
    });
    return out;
}

std::string scrub(std::string text, const std::vector<fs::path>& folders) {
    for (const auto& folder : folders) {
        if (folder.empty()) continue;
        auto plain = folder.u8string();
        std::string a(plain.begin(), plain.end());
        while (!a.empty() && (a.back() == '\\' || a.back() == '/')) a.pop_back();
        if (a.size() < 4) continue;
        std::string escaped;
        for (const char c : a) { if (c == '\\') escaped += '\\'; escaped += c; }
        for (const auto& needle : {escaped, a})
            for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 8))
                text.replace(at, needle.size(), "<folder>");
    }
    return text;
}

std::string flags_text(const Session& s) {
    std::string out = "在遊戲中標記的翻譯（" + std::to_string(s.flags.size()) + " 筆，按了 " + std::to_string(s.flag_presses) + " 次）\r\n";
    out += "請在下方說明哪裡有問題，或建議的譯法。\r\n\r\n";
    for (const auto& f : s.flags) {
        std::string text = f.text;
        for (auto& c : text) if (c == '\r' || c == '\n') c = ' ';
        out += f.key + "\t" + text + "\r\n";
    }
    return out;
}

bool write_zip(const fs::path& file, const std::vector<std::pair<std::string, std::string>>& files) {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[i] = c;
    }
    const auto crc32 = [&](const std::string& d) {
        uint32_t c = 0xFFFFFFFFu;
        for (const unsigned char b : d) c = table[(c ^ b) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    };
    std::string out, central;
    const auto u16 = [](std::string& s, uint32_t v) { s += char(v & 0xFF); s += char((v >> 8) & 0xFF); };
    const auto u32 = [&](std::string& s, uint32_t v) { u16(s, v & 0xFFFF); u16(s, v >> 16); };
    for (const auto& [name, data] : files) {
        if (data.size() > 0xFFFFFFF0u || out.size() > 0xFFFFFFF0u) return false;
        const uint32_t crc = crc32(data), offset = uint32_t(out.size()), size = uint32_t(data.size());
        // version 20, flag 0x800 (UTF-8 name), stored, time 00:00, date 2026-01-01
        const uint32_t date = ((2026 - 1980) << 9) | (1 << 5) | 1;
        u32(out, 0x04034B50); u16(out, 20); u16(out, 0x800); u16(out, 0); u16(out, 0); u16(out, date);
        u32(out, crc); u32(out, size); u32(out, size); u16(out, uint32_t(name.size())); u16(out, 0);
        out += name; out += data;
        u32(central, 0x02014B50); u16(central, 20); u16(central, 20); u16(central, 0x800); u16(central, 0); u16(central, 0);
        u16(central, date); u32(central, crc); u32(central, size); u32(central, size); u16(central, uint32_t(name.size()));
        u16(central, 0); u16(central, 0); u16(central, 0); u16(central, 0); u32(central, 0); u32(central, offset);
        central += name;
    }
    const uint32_t at = uint32_t(out.size());
    out += central;
    u32(out, 0x06054B50); u16(out, 0); u16(out, 0); u16(out, uint32_t(files.size())); u16(out, uint32_t(files.size()));
    u32(out, uint32_t(central.size())); u32(out, at); u16(out, 0);
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    f.write(out.data(), std::streamsize(out.size()));
    return bool(f);
}
}
