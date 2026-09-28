#include "ironpulse/protocol/register_decoder.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace ironpulse::protocol {

std::vector<ReadBlock> plan_reads(const std::vector<core::SensorConfig>& sensors,
                                  std::uint16_t max_quantity,
                                  std::uint16_t max_gap) {
    std::vector<ReadBlock> blocks;

    for (const auto type : {core::RegisterType::holding, core::RegisterType::input}) {
        std::vector<std::size_t> order;
        for (std::size_t i = 0; i < sensors.size(); ++i) {
            if (sensors[i].register_type == type) {
                order.push_back(i);
            }
        }
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return sensors[a].address < sensors[b].address;
        });

        ReadBlock current;
        bool open = false;
        for (const std::size_t index : order) {
            const auto& sensor = sensors[index];
            const std::uint32_t begin = sensor.address;
            const std::uint32_t end = begin + core::register_count(sensor.data_type);  // exclusive

            if (open) {
                const std::uint32_t block_end = std::uint32_t{current.start_address} + current.quantity;
                const std::uint32_t gap = begin > block_end ? begin - block_end : 0;
                const std::uint32_t merged_end = std::max(block_end, end);
                if (gap <= max_gap && merged_end - current.start_address <= max_quantity) {
                    current.quantity = static_cast<std::uint16_t>(merged_end - current.start_address);
                    current.sensor_indices.push_back(index);
                    continue;
                }
                blocks.push_back(std::move(current));
                current = ReadBlock{};
            }

            current.register_type = type;
            current.start_address = sensor.address;
            current.quantity = static_cast<std::uint16_t>(end - begin);
            current.sensor_indices = {index};
            open = true;
        }
        if (open) {
            blocks.push_back(std::move(current));
        }
    }
    return blocks;
}

double decode_raw(std::span<const std::uint16_t> registers, core::DataType type, core::WordOrder order) {
    if (type == core::DataType::uint16) {
        return static_cast<double>(registers[0]);
    }
    if (type == core::DataType::int16) {
        return static_cast<double>(static_cast<std::int16_t>(registers[0]));
    }

    const std::uint16_t first = registers[0];
    const std::uint16_t second = registers[1];
    const std::uint32_t high = order == core::WordOrder::big ? first : second;
    const std::uint32_t low = order == core::WordOrder::big ? second : first;
    const std::uint32_t bits = (high << 16) | low;

    switch (type) {
        case core::DataType::uint32:
            return static_cast<double>(bits);
        case core::DataType::int32:
            return static_cast<double>(static_cast<std::int32_t>(bits));
        case core::DataType::float32:
            return static_cast<double>(std::bit_cast<float>(bits));
        case core::DataType::uint16:
        case core::DataType::int16:
            break;
    }
    return 0.0;
}

std::optional<double> extract_value(const ReadBlock& block,
                                    std::span<const std::uint16_t> registers,
                                    const core::SensorConfig& sensor) {
    const std::size_t offset = static_cast<std::size_t>(sensor.address - block.start_address);
    const std::size_t count = core::register_count(sensor.data_type);
    if (sensor.address < block.start_address || offset + count > registers.size()) {
        return std::nullopt;
    }
    const double raw = decode_raw(registers.subspan(offset, count), sensor.data_type, sensor.word_order);
    const double value = raw * sensor.scale + sensor.offset;
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

}  // namespace ironpulse::protocol
