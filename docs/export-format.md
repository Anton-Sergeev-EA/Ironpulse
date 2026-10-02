# Export segment format

With `export.enabled`, Ironpulse writes every sensor reading to segment files
in `export.directory`. This page is the byte-level description, so the files
can be read without Ironpulse — from Python, Spark, a historian importer or a
script. `ironpulse-export` (bundled) and
[`tools/export_reader/read_segment.py`](../tools/export_reader/read_segment.py)
(standard library only) are reference readers.

## Files

```
export/
├── telemetry-20261002T111014-938Z-000001.ipseg        finished, safe to ship
├── telemetry-20261002T112507-114Z-000002.ipseg
└── telemetry-20261002T114003-560Z-000003.ipseg.part   being written — skip it
```

- Names start with the UTC creation time, so sorting by name is chronological.
- A segment is written as `*.ipseg.part` and renamed to `*.ipseg` when it is
  full (`export.segment_max_mb`) or Ironpulse stops. Anything that ships
  segments elsewhere (rclone, `aws s3 sync --exclude "*.part"`, a cron job)
  should take only `*.ipseg`.
- After a crash, the next start renames leftover `.part` files to `.ipseg`; the
  batches that reached the disk stay readable, and the reader reports the cut-off
  last batch.
- Ironpulse never deletes segments: removing them once they are shipped is up to
  you (for example `find export -name '*.ipseg' -mtime +7 -delete`).

## Layout

All integers are little-endian. Offsets are from the start of the file.

```
┌──────────────────────────┐ 0
│ SegmentHeader (32 bytes) │
├──────────────────────────┤ 32
│ tag table (JSON, UTF-8)  │ tag_table_bytes
├──────────────────────────┤
│ zero padding             │ up to a multiple of 8
├──────────────────────────┤
│ batch                    │ BatchHeader (16) + count × Sample (24)
│ batch                    │
│ …                        │
└──────────────────────────┘
```

Every batch starts on an 8-byte boundary, so a memory-mapped file can be read in
place.

### SegmentHeader — 32 bytes

| Offset | Type   | Field              | Value                                  |
|-------:|--------|--------------------|----------------------------------------|
| 0      | u32    | `magic`            | `0x47535049` — the bytes `IPSG`        |
| 4      | u16    | `version`          | `1`                                    |
| 6      | u16    | `flags`            | `0`                                    |
| 8      | i64    | `created_ms`       | Unix time in milliseconds, UTC         |
| 16     | u32    | `tag_table_bytes`  | length of the tag table                |
| 20     | u32    | `tag_table_crc32c` | CRC-32C of the tag table bytes         |
| 24     | u64    | `reserved`         | `0`                                    |

### Tag table

Maps the numeric `tag_id` in each sample to the sensor it came from. Every
segment carries its own copy, so old segments stay readable after sensors are
added, removed or reordered in the configuration.

```json
{
  "format": "ironpulse-export",
  "tags": [
    {"id": 1, "sensor": "winding_temp", "device": "transformer_01",
     "name": "Winding temperature", "unit": "°C"},
    {"id": 3, "sensor": "bearing_vibration", "device": "pump_02",
     "name": "Bearing vibration", "unit": "mm/s"}
  ]
}
```

Ids are assigned 1…N in configuration order when Ironpulse starts. They are
stable within a segment, **not** across restarts with a changed configuration —
always resolve them through the segment's own table.

### BatchHeader — 16 bytes

| Offset | Type | Field            | Value                                   |
|-------:|------|------------------|-----------------------------------------|
| 0      | u32  | `magic`          | `0x434C5041` — the bytes `APLC`         |
| 4      | u16  | `version`        | `2`                                     |
| 6      | u16  | `flags`          | `0`                                     |
| 8      | u32  | `count`          | number of samples that follow           |
| 12     | u32  | `payload_crc32c` | CRC-32C of the `count × 24` sample bytes |

### Sample — 24 bytes

| Offset | Type | Field          | Meaning                                           |
|-------:|------|----------------|---------------------------------------------------|
| 0      | u64  | `timestamp_ms` | Unix time in milliseconds, UTC                    |
| 8      | f64  | `value`        | engineering units (scale and offset applied)      |
| 16     | u32  | `tag_id`       | key into the tag table                            |
| 20     | u8   | `quality`      | `0` = good; other values reserved                 |
| 21     | u8×3 | padding        | `0`                                               |

In Python: `struct.Struct("<QdIB3x")`; in NumPy:
`np.dtype([("ts", "<u8"), ("value", "<f8"), ("tag", "<u4"), ("quality", "u1"), ("_", "V3")])`.

## Checksums

CRC-32C (Castagnoli, reflected polynomial `0x82F63B78`, initial value and final
XOR `0xFFFFFFFF`) — the variant computed by the x86 SSE4.2 `crc32` instruction,
used by iSCSI, ext4 and Kafka. Check value: `crc32c("123456789") = 0xE3069283`.

## Reading robustly

1. Check the segment magic and version; stop if either is wrong.
2. Verify the tag table CRC and parse it.
3. From the first 8-byte boundary after the table, repeat:
   - fewer than 16 bytes left, or the batch extends past the end of the file →
     the last batch was cut off; stop;
   - wrong batch magic or version → the region is damaged; advance 8 bytes and
     try again (batches always start on an 8-byte boundary);
   - payload CRC mismatch → skip `16 + count × 24` bytes and continue;
   - otherwise the samples are good.

## Version history

| Batch version | Change |
|---------------|--------|
| 1 | apollonian_core_ingestor: 12-byte header, IEEE CRC-32 in software but CRC-32C with SSE4.2 (unreadable across machines). Not produced by Ironpulse. |
| 2 | 16-byte header, CRC-32C everywhere, samples 8-byte aligned. |
