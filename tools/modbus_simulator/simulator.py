#!/usr/bin/env python3
"""
Modbus TCP device simulator for ironpulse demos.

Emulates two industrial sensors on a single Modbus TCP endpoint:
  - unit 1: transformer winding temperature (holding register 0), normally
    fluctuating around 65 degrees, with periodic injected spikes
  - unit 2: bearing vibration (holding register 0), normally fluctuating
    around 2.0 mm/s, with periodic injected spikes

Requires: pymodbus >= 3.6
    pip install pymodbus --break-system-packages

Run:
    python3 simulator.py --port 5020
"""

import argparse
import asyncio
import logging
import random

from pymodbus.datastore import (
    ModbusSequentialDataBlock,
    ModbusServerContext,
    ModbusSlaveContext,
)
from pymodbus.server import StartAsyncTcpServer

logging.basicConfig(level=logging.INFO, format="%(asctime)s [simulator] %(message)s")
log = logging.getLogger(__name__)


class SensorProfile:
    """Generates a plausible sensor value, with occasional anomaly spikes."""

    def __init__(self, name: str, baseline: float, noise: float, spike_multiplier: float,
                 spike_probability: float = 0.03):
        self.name = name
        self.baseline = baseline
        self.noise = noise
        self.spike_multiplier = spike_multiplier
        self.spike_probability = spike_probability

    def next_value(self) -> int:
        if random.random() < self.spike_probability:
            value = self.baseline * self.spike_multiplier
            log.warning("%s: injecting anomaly spike -> %.1f", self.name, value)
        else:
            value = self.baseline + random.uniform(-self.noise, self.noise)
        return max(0, int(round(value)))


async def update_loop(context: ModbusServerContext, profiles: dict[int, SensorProfile]):
    while True:
        for unit_id, profile in profiles.items():
            value = profile.next_value()
            slave = context[unit_id]
            slave.setValues(3, 0, [value])  # function code 3 = holding registers
        await asyncio.sleep(1)


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=5020)
    args = parser.parse_args()

    profiles = {
        1: SensorProfile("transformer_temp_01", baseline=65.0, noise=1.5, spike_multiplier=2.2),
        2: SensorProfile("bearing_vibration_02", baseline=2.0, noise=0.2, spike_multiplier=6.0),
    }

    slaves = {
        unit_id: ModbusSlaveContext(
            hr=ModbusSequentialDataBlock(0, [0] * 16),
            zero_mode=True,
        )
        for unit_id in profiles
    }
    context = ModbusServerContext(slaves=slaves, single=False)

    log.info("Starting Modbus TCP simulator on %s:%d (units: %s)", args.host, args.port,
              list(profiles.keys()))

    server_task = asyncio.create_task(
        StartAsyncTcpServer(context=context, address=(args.host, args.port))
    )
    updater_task = asyncio.create_task(update_loop(context, profiles))

    await asyncio.gather(server_task, updater_task)


if __name__ == "__main__":
    asyncio.run(main())
