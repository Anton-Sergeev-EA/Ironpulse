#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <nmmintrin.h>  // SSE4.2 CRC32C instructions
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#endif

namespace ironpulse::ingest {

// The wire format is the in-memory layout of the structs below, so it is
// only portable between little-endian machines — every platform Ironpulse
// targets (x86-64, ARM64) is one.
static_assert(std::endian::native == std::endian::little, "the export format assumes a little-endian host");

/// One reading in the export stream: 24 bytes, no implicit padding.
struct alignas(8) TelemetrySample {
    std::uint64_t timestamp_ms;  // Unix epoch, milliseconds (UTC)
    double value;                // engineering units, scale/offset already applied
    std::uint32_t tag_id;        // index into the segment's tag table
    std::uint8_t quality = 0;    // 0 = good; other values reserved
    std::uint8_t reserved[3]{};  // explicit padding, always zero
};
static_assert(sizeof(TelemetrySample) == 24);
static_assert(std::is_trivially_copyable_v<TelemetrySample>);
static_assert(std::is_standard_layout_v<TelemetrySample>);

/// Prefix of every batch. 16 bytes, so a batch that starts on an 8-byte
/// boundary has its samples on an 8-byte boundary too and can be read in
/// place without copying.
struct BatchHeader {
    std::uint32_t magic;           // "APLC" read as a little-endian uint32
    std::uint16_t version;         // format version, see Serializer::kVersion
    std::uint16_t flags;           // reserved, zero
    std::uint32_t count;           // number of samples that follow
    std::uint32_t payload_crc32c;  // CRC-32C (Castagnoli) of the sample bytes
};
static_assert(sizeof(BatchHeader) == 16);

/// Why a buffer could not be decoded as a batch.
enum class DecodeStatus {
    ok,
    too_short,            // fewer bytes than a header
    bad_magic,            // not a batch, or the stream is out of sync
    unsupported_version,  // written by a newer format version
    truncated,            // header is fine but the samples are cut off
    crc_mismatch,         // samples were damaged after they were written
    misaligned,           // valid, but not 8-byte aligned for an in-place view
};

namespace detail {

inline constexpr std::uint32_t kCrc32cPolynomial = 0x82F63B78U;  // reflected 0x1EDC6F41

constexpr std::array<std::uint32_t, 256> make_crc32c_table() noexcept {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t crc = i;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) != 0 ? (crc >> 1) ^ kCrc32cPolynomial : crc >> 1;
        }
        table[i] = crc;
    }
    return table;
}

inline constexpr std::array<std::uint32_t, 256> kCrc32cTable = make_crc32c_table();

}  // namespace detail

/// Zero-allocation encoder/decoder for batches of TelemetrySample.
class Serializer {
public:
    static constexpr std::uint32_t kMagic = 0x434C5041;  // bytes 'A' 'P' 'L' 'C'
    static constexpr std::uint16_t kVersion = 2;         // v1 had a 12-byte header and IEEE CRC-32

    [[nodiscard]] static constexpr std::size_t required_buffer_size(std::size_t sample_count) noexcept {
        return sizeof(BatchHeader) + sample_count * sizeof(TelemetrySample);
    }

    /// Writes one batch into `out`. Returns the number of bytes written, or
    /// 0 if `out` is too small (nothing is written in that case).
    static std::size_t serialize(std::span<const TelemetrySample> samples, std::span<std::uint8_t> out) noexcept {
        const std::size_t total = required_buffer_size(samples.size());
        if (out.size() < total || samples.size() > UINT32_MAX) {
            return 0;
        }
        const std::size_t payload_bytes = samples.size_bytes();
        const BatchHeader header{
            .magic = kMagic,
            .version = kVersion,
            .flags = 0,
            .count = static_cast<std::uint32_t>(samples.size()),
            .payload_crc32c = crc32c(samples.data(), payload_bytes),
        };
        std::memcpy(out.data(), &header, sizeof(header));
        if (payload_bytes > 0) {
            std::memcpy(out.data() + sizeof(header), samples.data(), payload_bytes);
        }
        return total;
    }

    /// Allocating convenience overload for code off the hot path.
    [[nodiscard]] static std::vector<std::uint8_t> serialize(std::span<const TelemetrySample> samples) {
        std::vector<std::uint8_t> buffer(required_buffer_size(samples.size()));
        serialize(samples, buffer);
        return buffer;
    }

    /// Reads and sanity-checks the header at the start of `data` without
    /// touching the samples. On success, `batch_bytes` is the full size of
    /// the batch (header + samples), which may exceed `data.size()`.
    [[nodiscard]] static DecodeStatus peek(std::span<const std::uint8_t> data,
                                           BatchHeader& header,
                                           std::size_t& batch_bytes) noexcept {
        if (data.size() < sizeof(BatchHeader)) {
            return DecodeStatus::too_short;
        }
        std::memcpy(&header, data.data(), sizeof(header));
        if (header.magic != kMagic) {
            return DecodeStatus::bad_magic;
        }
        if (header.version != kVersion) {
            return DecodeStatus::unsupported_version;
        }
        batch_bytes = required_buffer_size(header.count);
        return data.size() < batch_bytes ? DecodeStatus::truncated : DecodeStatus::ok;
    }

    /// Validates the batch at the start of `data` and returns a view of its
    /// samples inside `data` — no copy. Requires the samples to be 8-byte
    /// aligned (true for buffers from the allocator and for batches read at
    /// an 8-byte file offset); otherwise returns `misaligned` and the caller
    /// should use decode_copy().
    [[nodiscard]] static DecodeStatus decode_view(std::span<const std::uint8_t> data,
                                                  std::span<const TelemetrySample>& samples) noexcept {
        BatchHeader header{};
        std::size_t batch_bytes = 0;
        if (const auto status = peek(data, header, batch_bytes); status != DecodeStatus::ok) {
            return status;
        }
        const std::uint8_t* payload = data.data() + sizeof(BatchHeader);
        const std::size_t payload_bytes = batch_bytes - sizeof(BatchHeader);
        if (crc32c(payload, payload_bytes) != header.payload_crc32c) {
            return DecodeStatus::crc_mismatch;
        }
        if (reinterpret_cast<std::uintptr_t>(payload) % alignof(TelemetrySample) != 0) {
            return DecodeStatus::misaligned;
        }
        samples = {reinterpret_cast<const TelemetrySample*>(payload), header.count};
        return DecodeStatus::ok;
    }

    /// Validates the batch at the start of `data` and appends its samples to
    /// `out`. Works for any alignment.
    [[nodiscard]] static DecodeStatus decode_copy(std::span<const std::uint8_t> data,
                                                  std::vector<TelemetrySample>& out) {
        BatchHeader header{};
        std::size_t batch_bytes = 0;
        if (const auto status = peek(data, header, batch_bytes); status != DecodeStatus::ok) {
            return status;
        }
        const std::uint8_t* payload = data.data() + sizeof(BatchHeader);
        const std::size_t payload_bytes = batch_bytes - sizeof(BatchHeader);
        if (crc32c(payload, payload_bytes) != header.payload_crc32c) {
            return DecodeStatus::crc_mismatch;
        }
        const std::size_t first = out.size();
        out.resize(first + header.count);
        if (payload_bytes > 0) {
            std::memcpy(out.data() + first, payload, payload_bytes);
        }
        return DecodeStatus::ok;
    }

    /// CRC-32C (Castagnoli), the polynomial the SSE4.2 CRC32 instruction
    /// implements. Uses the instruction when the CPU has it and an
    /// equivalent table-driven implementation otherwise, so a batch written
    /// on one machine verifies on any other.
    [[nodiscard]] static std::uint32_t crc32c(const void* data, std::size_t length) noexcept {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
#if defined(__x86_64__) || defined(_M_X64)
        if (has_sse42()) {
            return crc32c_sse42(bytes, length);
        }
#endif
        return crc32c_software(bytes, length);
    }

    /// Portable CRC-32C; public so tests can check it against the hardware path.
    [[nodiscard]] static std::uint32_t crc32c_software(const std::uint8_t* data, std::size_t length) noexcept {
        std::uint32_t crc = 0xFFFFFFFFU;
        for (std::size_t i = 0; i < length; ++i) {
            crc = detail::kCrc32cTable[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFU;
    }

    /// True when crc32c() uses the SSE4.2 instruction on this machine.
    [[nodiscard]] static bool hardware_crc_available() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        return has_sse42();
#else
        return false;
#endif
    }

private:
#if defined(__x86_64__) || defined(_M_X64)
    // SSE4.2 is a run-time CPU feature, not implied by x86-64: some VMs and
    // low-power cores lack it. The instruction path is compiled with a
    // per-function target attribute (no global -msse4.2 needed) and only
    // entered after a CPUID check, so the binary never raises SIGILL.
    [[nodiscard]] static bool has_sse42() noexcept {
        static const bool supported = [] {
#if defined(_MSC_VER)
            int info[4] = {0, 0, 0, 0};
            __cpuid(info, 1);
            return (info[2] & (1 << 20)) != 0;  // ECX bit 20
#else
            return __builtin_cpu_supports("sse4.2") != 0;
#endif
        }();
        return supported;
    }

#if defined(__GNUC__) || defined(__clang__)
    __attribute__((target("sse4.2")))
#endif
    static std::uint32_t
    crc32c_sse42(const std::uint8_t* data, std::size_t length) noexcept {
        std::uint64_t crc = 0xFFFFFFFFU;
        while (length >= 8) {
            std::uint64_t chunk = 0;
            std::memcpy(&chunk, data, sizeof(chunk));
            crc = _mm_crc32_u64(crc, chunk);
            data += 8;
            length -= 8;
        }
        auto crc32 = static_cast<std::uint32_t>(crc);
        while (length > 0) {
            crc32 = _mm_crc32_u8(crc32, *data);
            ++data;
            --length;
        }
        return crc32 ^ 0xFFFFFFFFU;
    }
#endif
};

}  // namespace ironpulse::ingest
