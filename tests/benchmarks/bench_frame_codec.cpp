#include <benchmark/benchmark.h>

#include "ironpulse/protocol/frame_codec.hpp"

using namespace ironpulse::protocol;

static void BM_EncodeRequest(benchmark::State& state) {
    ModbusRequest request{
        .transaction_id = 1,
        .unit_id = 1,
        .function = FunctionCode::read_holding_registers,
        .start_address = 0,
        .quantity_or_value = 10,
    };
    for (auto _ : state) {
        auto bytes = encode_request(request);
        benchmark::DoNotOptimize(bytes);
    }
}
BENCHMARK(BM_EncodeRequest);

// Decoding is the hotter path in practice — it runs once per poll per
// device, whereas encoding is the same cost but decode also has to walk
// the register payload, so it's worth measuring separately.
static void BM_DecodeResponse(benchmark::State& state) {
    const auto register_count = static_cast<std::size_t>(state.range(0));
    std::vector<std::uint8_t> raw = {
        0x00,
        0x01,  // transaction id
        0x00,
        0x00,  // protocol id
        0x00,
        static_cast<std::uint8_t>(3 + register_count * 2),  // length
        0x01,                                               // unit id
        0x03,                                               // function code
        static_cast<std::uint8_t>(register_count * 2),      // byte count
    };
    for (std::size_t i = 0; i < register_count; ++i) {
        raw.push_back(0x12);
        raw.push_back(0x34);
    }

    for (auto _ : state) {
        auto response = decode_response(raw);
        benchmark::DoNotOptimize(response);
    }
}
BENCHMARK(BM_DecodeResponse)->Arg(1)->Arg(10)->Arg(50)->Arg(125);  // 125 = Modbus max per request
