"""Offline PE identity and candidate-byte extraction; never executes the input."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

EXPECTED = 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'

def inspect(path):
    data = Path(path).read_bytes()
    def unpack(fmt, offset):
        return struct.unpack_from('<' + fmt, data, offset)[0]
    if data[:2] != b'MZ':
        raise ValueError('Not MZ')
    pe = unpack('I', 0x3c)
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError('Not PE')
    opt = pe + 24
    if unpack('H', opt) != 0x10b:
        raise ValueError('Expected PE32')
    sections = []
    table = opt + unpack('H', pe + 20)
    for i in range(unpack('H', pe + 6)):
        off = table + i * 40
        sections.append(dict(name=data[off:off+8].rstrip(b'\0').decode('ascii', 'replace'),
                             virtual_size=unpack('I', off+8), rva=unpack('I', off+12),
                             raw_size=unpack('I', off+16), raw_offset=unpack('I', off+20)))
    digest = hashlib.sha256(data).hexdigest()
    markers = []
    start = 0
    while (start := data.find(b'HOODLUM', start)) >= 0:
        markers.append(start)
        start += 7
    return dict(schema_version=1, sha256=digest, size=len(data),
                matches_expected=digest == EXPECTED and len(data) == 4800512,
                machine=hex(unpack('H', pe+4)), format='PE32',
                preferred_image_base=hex(unpack('I', opt+28)),
                entry_rva=hex(unpack('I', opt+16)), sections=sections,
                hoodlum_file_offsets=markers,
                evidence_method='Python stdlib PE headers + SHA-256; no execution',
                version_interpretation='Not established by PE identity; see community leads')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('binary', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = inspect(args.binary)
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8')
    else:
        print(text)
    raise SystemExit(0 if result['matches_expected'] else 2)
