#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ironpulse/core/config.hpp"

namespace ironpulse::protocol {

/// One Modbus read request covering one or more sensors' registers.
struct ReadBlock {
    core::RegisterType register_type = core::RegisterType::holding;
    std::uint16_t start_address = 0;
    std::uint16_t quantity = 0;
    /// Indices into the device's sensor list served by this block.
    std::vector<std::size_t> sensor_indices;
};

/// Groups a device's sensors into as few read requests as possible.
///
/// Polling ten sensors with ten round-trips is slow and, on serial
/// gateways, can exceed the poll interval; reading one contiguous range
/// is what every SCADA system does. Sensors are merged into a block while
/// the block stays within `max_quantity` registers (125 is the Modbus
/// limit for function codes 0x03/0x04) and the unused gap between
/// neighbours is at most `max_gap` registers — reading a few unused
/// registers is cheaper than an extra round-trip, but some devices reject
/// reads that touch unmapped addresses, so the gap is bounded.
[[nodiscard]] std::vector<ReadBlock> plan_reads(const std::vector<core::SensorConfig>& sensors,
                                                std::uint16_t max_quantity = 125,
                                                std::uint16_t max_gap = 8);

/// Interprets 1 or 2 raw registers as the given data type.
[[nodiscard]] double decode_raw(std::span<const std::uint16_t> registers,
                                core::DataType type,
                                core::WordOrder order);

/// Extracts one sensor's engineering value from a block's response:
/// decodes its registers and applies scale and offset. Returns
/// std::nullopt if the response is too short or the value is not a
/// finite number (e.g. a float register holding NaN).
[[nodiscard]] std::optional<double> extract_value(const ReadBlock& block,
                                                  std::span<const std::uint16_t> registers,
                                                  const core::SensorConfig& sensor);

}  // namespace ironpulse::protocol
