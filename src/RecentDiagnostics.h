#pragma once
#include "Utf8.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

// Complete, already-formatted records. Recording never allocates or performs I/O.
class RecentDiagnosticsBuffer {
public:
    static constexpr std::size_t capacity = 32, lineBytes = 2048;
    static constexpr std::size_t maximumTextBytes = capacity * (lineBytes - 1) * 4 + 512;
    void clear() noexcept { count_ = 0; next_ = 0; overwritten_ = 0; }
    bool append(std::string_view line) noexcept {
        if (line.empty() || line.size() >= lineBytes) return false;
        auto& record = records_[next_];
        std::memcpy(record.text.data(), line.data(), line.size());
        record.length = line.size();
        next_ = (next_ + 1) % capacity;
        if (count_ < capacity) ++count_;
        else if (overwritten_ != (std::numeric_limits<std::uint64_t>::max)()) ++overwritten_;
        return true;
    }
    std::optional<std::string_view> format(std::span<char> output) const noexcept {
        if (output.empty()) return std::nullopt;
        const int length = std::snprintf(output.data(), output.size(),
            "RedLight recent diagnostics\r\nRetained records: %zu/%zu\r\nEarlier records overwritten: %llu\r\n"
            "Oldest retained first; process memory only.\r\nNon-UTF-8 records escape non-ASCII/control bytes as \\xHH.\r\n\r\n",
            count_, capacity, static_cast<unsigned long long>(overwritten_));
        if (length < 0 || static_cast<std::size_t>(length) >= output.size()) return std::nullopt;
        auto used = static_cast<std::size_t>(length);
        constexpr char digits[] = "0123456789ABCDEF";
        for (std::size_t index = 0; index < count_; ++index) {
            const auto& record = records_[(next_ + capacity - count_ + index) % capacity];
            const std::string_view line(record.text.data(), record.length);
            const bool escape = !IsValidUtf8(line) || line.find('\0') != std::string_view::npos;
            for (const unsigned char character : line) {
                const bool hex = escape && (character >= 0x80 || character == '\\' ||
                    (character < 0x20 && character != '\r' && character != '\n' && character != '\t') || character == 0x7f);
                const std::size_t required = hex ? 4 : 1;
                if (required > output.size() - used) return std::nullopt;
                if (hex) {
                    output[used++] = '\\'; output[used++] = 'x';
                    output[used++] = digits[character >> 4]; output[used++] = digits[character & 15];
                } else output[used++] = static_cast<char>(character);
            }
        }
        return std::string_view(output.data(), used);
    }
private:
    struct Record { std::array<char, lineBytes> text{}; std::size_t length = 0; };
    std::array<Record, capacity> records_{};
    std::size_t count_ = 0, next_ = 0;
    std::uint64_t overwritten_ = 0;
};
