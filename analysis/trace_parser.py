from __future__ import annotations

import struct
import subprocess
import sys

from urllib.parse import urlparse
from urllib.request import urlopen
from typing import BinaryIO, Iterator, Tuple

# oracleGeneral binary record layout (24 bytes, little-endian):
#   uint32_t clock_time        offset 0
#   uint64_t obj_id            offset 4
#   uint32_t obj_size          offset 12
#   int64_t  next_access_vtime offset 16
RECORD_FORMAT = "<IQIq"
RECORD_SIZE = struct.calcsize(RECORD_FORMAT)  # 24
READ_CHUNK_RECORDS = 16384
READ_CHUNK_SIZE = RECORD_SIZE * READ_CHUNK_RECORDS

def _open_trace(tracepath: str) -> tuple[BinaryIO, BinaryIO]:
    is_url = tracepath.startswith("http://") or tracepath.startswith("https://")
    
    if tracepath.endswith(".zst"):
        import zstandard as zstd
        fh = urlopen(tracepath) if is_url else open(tracepath, "rb")
        dctx = zstd.ZstdDecompressor()
        return dctx.stream_reader(fh), fh

    fh = urlopen(tracepath) if is_url else open(tracepath, "rb")
    return fh, fh

def trace_name(tracepath: str) -> str:
    """Return a stable display key for a local path or URL."""
    if tracepath.startswith("http://") or tracepath.startswith("https://"):
        return urlparse(tracepath).path.rstrip("/").split("/")[-1]
    return tracepath.rstrip("/").split("/")[-1]

def read_requests(tracepath: str) -> Iterator[Tuple[int, int, int]]:
    stream, fh = _open_trace(tracepath)
    try:
        # Read large chunks and decode many records at once to reduce Python IO overhead.
        # Keep a tiny remainder buffer for stream boundaries not aligned to RECORD_SIZE.
        remainder = b""
        while True:
            data = stream.read(READ_CHUNK_SIZE)
            if not data:
                break

            buf = remainder + data
            full_n = len(buf) // RECORD_SIZE
            full_bytes = full_n * RECORD_SIZE
            if full_bytes == 0:
                remainder = buf
                continue

            for clock_time, obj_id, obj_size, _ in struct.iter_unpack(
                RECORD_FORMAT, buf[:full_bytes]
            ):
                yield int(clock_time), int(obj_id), int(obj_size)

            remainder = buf[full_bytes:]
    finally:
        stream.close()
        if stream is not fh:
            fh.close()

