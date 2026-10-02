"""Minimal reader for Ironpulse export segments (docs/export-format.md).

    python3 read_segment.py telemetry-20261002T111014-938Z-000001.ipseg

Prints one CSV row per reading. Uses only the standard library; CRC-32C is
computed in pure Python, which is slow for large files — pip install crc32c
and swap in crc32c.crc32c() if that matters.
"""

import csv
import json
import struct
import sys
from datetime import datetime, timezone


def _crc32c_table():
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            crc = (crc >> 1) ^ 0x82F63B78 if crc & 1 else crc >> 1
        table.append(crc)
    return table


_TABLE = _crc32c_table()


def crc32c(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        crc = _TABLE[(crc ^ byte) & 0xFF] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF


SEGMENT_HEADER = struct.Struct("<IHHqIIQ")  # 32 bytes
BATCH_HEADER = struct.Struct("<IHHII")  # 16 bytes
SAMPLE = struct.Struct("<QdIB3x")  # 24 bytes
SEGMENT_MAGIC = 0x47535049  # "IPSG"
BATCH_MAGIC = 0x434C5041  # "APLC"


def read_segment(path):
    """Yields (timestamp_ms, tag, value, quality) for every verified reading."""
    with open(path, "rb") as f:
        data = f.read()
    magic, version, _flags, _created_ms, table_len, table_crc, _ = SEGMENT_HEADER.unpack_from(data, 0)
    if magic != SEGMENT_MAGIC or version != 1:
        raise ValueError("not an Ironpulse export segment (or a newer version)")
    table = data[32 : 32 + table_len]
    if crc32c(table) != table_crc:
        raise ValueError("tag table is damaged")
    tags = {t["id"]: t for t in json.loads(table)["tags"]}

    pos = (32 + table_len + 7) & ~7
    while pos + BATCH_HEADER.size <= len(data):
        magic, version, _flags, count, payload_crc = BATCH_HEADER.unpack_from(data, pos)
        if magic != BATCH_MAGIC or version != 2:
            pos += 8  # lost sync: look for the next batch on an 8-byte boundary
            continue
        start = pos + BATCH_HEADER.size
        end = start + count * SAMPLE.size
        if end > len(data):
            break  # last batch cut off (crash mid-write)
        if crc32c(data[start:end]) == payload_crc:
            for ts, value, tag_id, quality in SAMPLE.iter_unpack(data[start:end]):
                yield ts, tags.get(tag_id, {"sensor": f"tag_{tag_id}"}), value, quality
        pos = end


if __name__ == "__main__":
    out = csv.writer(sys.stdout)
    out.writerow(["timestamp", "sensor_id", "device_id", "value", "unit", "quality"])
    for path in sys.argv[1:]:
        for ts, tag, value, quality in read_segment(path):
            when = datetime.fromtimestamp(ts / 1000, tz=timezone.utc).isoformat(timespec="milliseconds")
            out.writerow([when, tag["sensor"], tag.get("device", ""), value, tag.get("unit", ""), quality])
