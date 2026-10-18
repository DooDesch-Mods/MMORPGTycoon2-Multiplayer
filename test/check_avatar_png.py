"""Independently verify actual C++-generated PNG chunks, DEFLATE and every RGBA pixel."""
from pathlib import Path
import argparse
import struct
import zlib

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
args = parser.parse_args()
cases = [('png1.png',1,1,1,1),('png128.png',128,128,128,128),
         ('resized-184x184.png',184,184,128,128),('resized-184x92.png',184,92,128,64),
         ('resized-92x184.png',92,184,64,128),('resized-512x512.png',512,512,128,128)]
for filename,source_width,source_height,width,height in cases:
    data = (args.directory/filename).read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    at = 8
    compressed = b''
    kinds = []
    while at < len(data):
        length = struct.unpack_from('>I',data,at)[0]
        kind = data[at+4:at+8]
        body = data[at+8:at+8+length]
        crc = struct.unpack_from('>I',data,at+8+length)[0]
        assert zlib.crc32(kind+body) == crc
        kinds.append(kind)
        if kind == b'IHDR':
            assert struct.unpack('>IIBBBBB',body) == (width,height,8,6,0,0,0)
        if kind == b'IDAT':
            compressed += body
        at += 12+length
    assert at == len(data) and kinds == [b'IHDR',b'IDAT',b'IEND']
    raw = zlib.decompress(compressed)
    expected = bytearray()
    for y in range(height):
        expected.append(0)
        sy = y*source_height//height
        for x in range(width):
            sx = x*source_width//width
            expected.extend((sx&255,sy&255,127,(sx+sy)&255))
    assert raw == expected
    print(f'PASS: {filename} {source_width}x{source_height}->{width}x{height} PNG CRC/DEFLATE and exact sampled RGBA pattern')
