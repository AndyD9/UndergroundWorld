"""Map documented community candidates to file-backed bytes, not semantic proof."""
import json
import sys
from pathlib import Path
from inspect_binary import inspect

path = Path(sys.argv[1])
manifest = inspect(path)
if not manifest['matches_expected']:
    raise SystemExit('Refusing unknown fingerprint')
data = path.read_bytes()
base = int(manifest['preferred_image_base'], 16)
candidates = {
    'GameFlowManager': 0x8654A4, 'PlayersByNumber': 0x8900AC,
    'Player_AutoPilotOn': 0x5FAE20, 'Player_AutoPilotOff': 0x5FAF10,
    'Player_SetInputMode': 0x605C70, 'SaveHotPosition': 0x8900CC,
    'JumpToHotPosition': 0x8900CD, 'Tweak_GameSpeed': 0x7A5730,
    'CarTypeInfoArray': 0x8A1CCC, 'GetCarTypeInfo_Game': 0x610110,
    'CompleatCarPhysicsInfo_BuildCarPhysicsInfo': 0x5AC4F0,
    'CarRenderInfo_TriggerEffect_Game': 0x615CC0,
}
rows = []
for name, va in candidates.items():
    rva = va - base
    row = dict(candidate=name, va=hex(va), rva=hex(rva), semantic_status='unverified')
    for section in manifest['sections']:
        delta = rva - section['rva']
        if 0 <= delta < max(section['virtual_size'], section['raw_size']):
            row['section'] = section['name']
            if delta < section['raw_size']:
                offset = section['raw_offset'] + delta
                row.update(file_offset=offset, bytes_hex=data[offset:offset+min(16, section['raw_size']-delta)].hex())
            else:
                row['file_offset'] = None
                row['observation'] = 'Virtual section tail; no initialized file bytes at this RVA'
            break
    else:
        row['observation'] = 'Outside sections'
    rows.append(row)
print(json.dumps(dict(sha256=manifest['sha256'], source='community-leads.md',
                      method='PE section mapping; 16 bytes maximum; no disassembly or execution',
                      candidates=rows), indent=2))
