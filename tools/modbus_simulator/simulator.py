#!/usr/bin/env python3
"""
Modbus TCP device simulator for ironpulse demos.

Emulates two industrial devices on a single Modbus TCP endpoint, using the
register layouts real equipment uses (see deploy/config/*.json for the
matching ironpulse configuration):

  unit 1 — transformer
    holding 0-1  winding temperature, float32 (big-endian words), °C
                 ~65 °C, occasional spikes above the 90 °C limit
    input   0    oil temperature, int16, 0.1 °C per count
                 ~55 °C, slowly drifting

  unit 2 — pump
    holding 0    bearing vibration, uint16, 0.01 mm/s per count
                 ~2.0 mm/s, occasional spikes
    holding 1    discharge pressure, uint16, 0.1 bar per count
                 ~6.0 bar, with occasional sustained drops

Requires: pymodbus 3.6.x (see requirements.txt)

Run:
    python3 simulator.py --port 5020
"""

from __future__ import annotations

import argparse
import asyncio
import logging
import math
import random
import struct
from dataclasses import dataclass, field

from pymodbus.datastore import (
    ModbusSequentialDataBlock,
    ModbusServerContext,
    ModbusSlaveContext,
)
from pymodbus.server import StartAsyncTcpServer

logging.basicConfig(level=logging.INFO, format="%(asctime)s [simulator] %(message)s")
log = logging.getLogger(__name__)

HOLDING = 3  # pymodbus "function code" selecting the holding-register table
INPUT = 4  # ... and the input-register table


@dataclass
class Signal:
    """A plausible sensor signal: noise around a baseline, plus anomalies.

    spike_probability: chance per tick of a one-sample spike
    drift_probability: chance per tick of starting a sustained excursion
    """

    name: str
    baseline: float
    noise: float
    spike_value: float
    spike_probability: float = 0.03
    drift_offset: float = 0.0
    drift_probability: float = 0.0
    drift_ticks: int = 20
    _drift_left: int = field(default=0, init=False)
    _tick: int = field(default=0, init=False)

    def next_value(self) -> float:
        self._tick += 1
        if random.random() < self.spike_probability:
            log.warning("%s: injecting spike -> %.2f", self.name, self.spike_value)
            return self.spike_value

        if self._drift_left == 0 and random.random() < self.drift_probability:
            self._drift_left = self.drift_ticks
            log.warning("%s: starting sustained excursion of %+.2f", self.name, self.drift_offset)

        value = self.baseline + random.uniform(-self.noise, self.noise)
        # A slow sine keeps the chart alive even without anomalies.
        value += self.noise * 0.5 * math.sin(self._tick / 15.0)
        if self._drift_left > 0:
            self._drift_left -= 1
            value += self.drift_offset
        return value


def float32_registers(value: float) -> list[int]:
    """Encodes a float as two big-endian 16-bit registers (ABCD)."""
    high, low = struct.unpack(">HH", struct.pack(">f", value))
    return [high, low]


def scaled_uint16(value: float, per_count: float) -> int:
    return max(0, min(0xFFFF, round(value / per_count)))


def scaled_int16(value: float, per_count: float) -> int:
    raw = max(-0x8000, min(0x7FFF, round(value / per_count)))
    return raw & 0xFFFF  # two's complement in an unsigned register


WINDING = Signal("winding_temp", baseline=65.0, noise=1.5, spike_value=142.0, spike_probability=0.02)
OIL = Signal(
    "oil_temp",
    baseline=55.0,
    noise=0.4,
    spike_value=55.0,
    spike_probability=0.0,
    drift_offset=6.0,
    drift_probability=0.01,
    drift_ticks=40,
)
VIBRATION = Signal("bearing_vibration", baseline=2.0, noise=0.2, spike_value=12.0, spike_probability=0.03)
PRESSURE = Signal(
    "pump_pressure",
    baseline=6.0,
    noise=0.15,
    spike_value=6.0,
    spike_probability=0.0,
    drift_offset=-3.0,
    drift_probability=0.01,
    drift_ticks=15,
)


async def update_loop(context: ModbusServerContext, interval: float) -> None:
    while True:
        transformer = context[1]
        transformer.setValues(HOLDING, 0, float32_registers(WINDING.next_value()))
        transformer.setValues(INPUT, 0, [scaled_int16(OIL.next_value(), 0.1)])

        pump = context[2]
        pump.setValues(
            HOLDING,
            0,
            [
                scaled_uint16(VIBRATION.next_value(), 0.01),
                scaled_uint16(PRESSURE.next_value(), 0.1),
            ],
        )
        await asyncio.sleep(interval)


def make_unit() -> ModbusSlaveContext:
    return ModbusSlaveContext(
        hr=ModbusSequentialDataBlock(0, [0] * 16),
        ir=ModbusSequentialDataBlock(0, [0] * 16),
        zero_mode=True,
    )


async def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=5020)
    parser.add_argument("--interval", type=float, default=0.5, help="seconds between value updates")
    args = parser.parse_args()

    context = ModbusServerContext(slaves={1: make_unit(), 2: make_unit()}, single=False)
    log.info("Starting Modbus TCP simulator on %s:%d (units 1: transformer, 2: pump)", args.host, args.port)

    await asyncio.gather(
        StartAsyncTcpServer(context=context, address=(args.host, args.port)),
        update_loop(context, args.interval),
    )


if __name__ == "__main__":
    asyncio.run(main())
