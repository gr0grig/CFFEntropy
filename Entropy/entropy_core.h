#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace entropy {
struct Row {
    std::string name;
    uint32_t number = 0;
    uint64_t offset = 0, declared = 0, available = 0;
    double value = 0;
};
struct Report { std::vector<Row> rows; bool truncated = false; };
inline uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
inline uint32_t u32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline double shannon(const uint8_t* p, size_t n) {
    if (!n) return 0;
    std::array<uint64_t, 256> counts{};
    for (size_t i = 0; i < n; ++i) ++counts[p[i]];
    double h = 0;
    for (auto count : counts) if (count) {
        const double probability = static_cast<double>(count) / static_cast<double>(n);
        h -= probability * std::log2(probability);
    }
    return (std::min)(8.0, (std::max)(0.0, h));
}
inline std::vector<double> map(const uint8_t* p, size_t size, size_t blocks = 256) {
    if (!blocks || blocks > 65536) throw std::invalid_argument("Invalid block count.");
    std::vector<double> result(blocks);
    for (uint64_t i = 0; i < result.size(); ++i) {
        const auto first = static_cast<size_t>((uint64_t(size) * i) / blocks);
        const auto last = static_cast<size_t>((uint64_t(size) * (i + 1)) / blocks);
        // Empty intervals have no entropy value.
        result[static_cast<size_t>(i)] = first == last ? -1.0 : shannon(p + first, last - first);
    }
    return result;
}
inline Report analyze(const uint8_t* p, size_t size) {
    auto require = [&](uint64_t off, uint64_t length) {
        if (off > size || length > size - off) throw std::runtime_error("Truncated or invalid PE headers.");
    };
    require(0, 64);
    if (!p || u16(p) != 0x5a4d) throw std::runtime_error("Not a PE file (missing MZ).");
    const uint64_t nt = u32(p + 60);
    require(nt, 24);
    if (u32(p + nt) != 0x4550) throw std::runtime_error("Not a PE file (missing PE signature).");
    const auto sections = u16(p + nt + 6);
    const auto optionalSize = u16(p + nt + 20);
    const auto optional = nt + 24;
    require(optional, optionalSize);
    if (optionalSize < 64 || (u16(p + optional) != 0x10b && u16(p + optional) != 0x20b))
        throw std::runtime_error("Invalid PE optional header.");
    const uint64_t table = optional + optionalSize;
    require(table, uint64_t(sections) * 40);
    uint64_t end = (std::max)(table + uint64_t(sections) * 40, uint64_t(u32(p + optional + 60)));
    Report result;
    result.truncated = end > size;
    for (uint32_t i = 0; i < sections; ++i) {
        const auto s = p + table + uint64_t(i) * 40;
        Row row;
        for (int j = 0; j < 8 && s[j]; ++j) row.name += s[j] >= 32 && s[j] <= 126 ? char(s[j]) : '?';
        if (row.name.empty()) row.name = "(unnamed)";
        row.number = i + 1;
        row.declared = u32(s + 16);
        row.offset = u32(s + 20);
        if (row.declared) {
            end = (std::max)(end, row.offset + row.declared);
            if (row.offset < size) row.available = (std::min)(row.declared, uint64_t(size) - row.offset);
            if (row.available != row.declared) result.truncated = true;
            if (row.available) row.value = shannon(p + row.offset, static_cast<size_t>(row.available));
        }
        result.rows.push_back(row);
    }
    if (end < size) {
        Row overlay;
        overlay.name = "Overlay";
        overlay.offset = end;
        overlay.declared = overlay.available = size - end;
        overlay.value = shannon(p + end, static_cast<size_t>(overlay.available));
        result.rows.push_back(overlay);
    }
    return result;
}
}
