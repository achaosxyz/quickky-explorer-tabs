"""Generate reviewed native entry templates; never load DLLs or download symbols.

Requires retained, SHA-verified reference images and MSVC dumpbin. Production has
no Python/disassembler dependency. Accessor-family checkers are preserved verbatim.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PERMISSIONS = 0xE0000000  # IMAGE_SCN_MEM_{READ,WRITE,EXECUTE}


class Image:
    def __init__(self, path):
        self.path = Path(path)
        self.raw = self.path.read_bytes()
        nt = struct.unpack_from('<I', self.raw, 60)[0]
        optional = nt + 24
        self.base = struct.unpack_from('<Q', self.raw, optional + 24)[0]
        self.data = bytearray(struct.unpack_from('<I', self.raw, optional + 56)[0])
        headers = struct.unpack_from('<I', self.raw, optional + 60)[0]
        self.data[:headers] = self.raw[:headers]
        table = optional + struct.unpack_from('<H', self.raw, nt + 20)[0]
        self.sections = []
        for i in range(struct.unpack_from('<H', self.raw, nt + 6)[0]):
            s = table + i * 40
            virtual, rva, length, offset = struct.unpack_from('<IIII', self.raw, s + 8)
            flags = struct.unpack_from('<I', self.raw, s + 36)[0]
            self.sections.append((rva, rva + max(virtual, length), flags & PERMISSIONS))
            self.data[rva:rva + length] = self.raw[offset:offset + length]
        pdata, size = struct.unpack_from('<II', self.raw, optional + 112 + 3 * 8)
        self.functions = {a: b - a for a, b, _ in
                          struct.iter_unpack('<III', self.data[pdata:pdata + size])}

    def permissions(self, target):
        for begin, end, flags in self.sections:
            if begin <= target < end:
                return flags
        raise ValueError(f'Address outside image sections: {target:x}')


def disassemble(tool, path, image, rva, length):
    env = dict(os.environ, _NT_SYMBOL_PATH='', _NT_ALT_SYMBOL_PATH='')
    text = subprocess.check_output(
        [str(tool), '/nologo', '/disasm:bytes',
         f'/range:0x{image.base + rva:x},0x{image.base + rva + length:x}', str(path)],
        env=env, timeout=30).decode('utf-8', errors='replace')
    entries = []
    for line in text.splitlines():
        match = re.match(r'^\s*([0-9A-F]{16}):\s+((?:[0-9A-F]{2} )+)\s*([a-z].*)$', line)
        if match:
            offset = int(match[1], 16) - image.base - rva
            if 0 <= offset < length:
                entries.append((offset, match[3]))
    if not entries or entries[0][0] != 0:
        raise ValueError(f'Cannot disassemble complete entry at {rva:x}')
    return entries


def entry(tool, path, image, rva, length, unwind, targets):
    code = bytearray(image.data[rva:rva + length])
    instructions = disassemble(tool, path, image, rva, length)
    references = []
    for i, (offset, assembly) in enumerate(instructions):
        end = instructions[i + 1][0] if i + 1 < len(instructions) else length
        instruction = code[offset:end]
        # Only decoded external branches and absolute RIP-relative memory
        # operands are candidates, never byte-search hits or field displacements.
        branch = ((len(instruction) == 5 and instruction[0] in (0xE8, 0xE9)) or
                  (len(instruction) == 6 and instruction[0] == 0x0F and
                   0x80 <= instruction[1] <= 0x8F))
        rip = re.search(r'\[([0-9A-F]{16})h\]', assembly)
        if not branch and not rip:
            continue
        operand = end - 4
        target = rva + end + struct.unpack_from('<i', code, operand)[0]
        if rip and int(rip[1], 16) != image.base + target:
            raise ValueError('RIP target does not agree with decoded instruction')
        if rva <= target < rva + length:
            continue  # Internal control flow stays byte-exact.
        flags = image.permissions(target)
        if not flags & 0x40000000 or (branch and flags & 0xA0000000 != 0x20000000):
            raise ValueError('Unexpected reference target permissions')
        identity = targets.setdefault(target, len(targets) + 1)
        if identity > 1024:
            raise ValueError('Reference identity budget exceeded')
        references.append((operand, end, flags, identity))
        code[operand:end] = bytes(4)
    return bytes(code), unwind, tuple(references)


def sample(source, record, research, tool, overrides):
    text = source.read_text(encoding='utf-8')
    descriptor = re.search(
        r'\n\s*(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+),\s*\{([^}]+)\},\s*([1-5])\}', text)
    if not descriptor:
        raise ValueError(f'Missing oracle descriptor: {source}')
    methods = [int(x, 16) for x in re.findall(r'0x[0-9a-f]+', descriptor[4])]
    fingerprints = re.findall(r'\{(0x[0-9a-f]+),(true|false),\{([^}]+)\}\}', text)
    if len(methods) != 13 or len(fingerprints) != 13:
        raise ValueError('Incomplete typed interface oracle')
    path = overrides.get(source.stem, record.get('ImagePath'))
    if not path:
        provenance = json.loads((research / 'server-returned' / source.stem /
                                 'provenance.json').read_text(encoding='utf-8'))
        path = provenance['SampleImagePath']
    image = Image(path)
    if hashlib.sha256(image.raw).hexdigest() != record['ActualSha256']:
        raise ValueError(f'Image provenance mismatch: {source.stem}, {path}')
    targets = {}
    entries = []
    with tempfile.TemporaryDirectory(prefix='qtabs-abi-') as directory:
        # No adjacent PDB or symbol path: disassembly must be reproducible offline.
        isolated = Path(directory) / 'sample.dll'
        isolated.write_bytes(image.raw)
        for slot, (length, unwind, digest) in enumerate(fingerprints):
            if slot == 8:
                entries.append(None)
                continue
            length, unwind = int(length, 16), unwind == 'true'
            expected = bytes(int(x, 16) for x in re.findall(r'0x[0-9a-f]+', digest))
            body = image.data[methods[slot]:methods[slot] + length]
            if hashlib.sha256(body).digest() != expected:
                raise ValueError(f'Oracle bytes mismatch: {source.stem}, slot {slot}')
            if (image.functions.get(methods[slot]) == length) != unwind:
                raise ValueError('Oracle unwind extent mismatch')
            entries.append(entry(tool, isolated, image, methods[slot], length, unwind, targets))
        window_rva = struct.unpack_from('<Q', image.data, int(descriptor[1], 16))[0] - image.base
        length = image.functions.get(window_rva)
        if length is None:
            # Complete reviewed AddRef leaf, not a search for a convenient return.
            leaf = image.data[window_rva:window_rva + 16]
            if leaf[:9] != bytes.fromhex('b801000000f00fc181') or leaf[13:] != bytes.fromhex('ffc0c3'):
                raise ValueError('Unreviewed window entry')
            length = 16
        window = entry(tool, isolated, image, window_rva, length,
                       window_rva in image.functions, targets)
    return source.stem, int(descriptor[5]), (tuple(entries), window)


def emit(rule, variants, path):
    old = path.read_text(encoding='utf-8')
    tail = old[old.index('const NativeAbiFamily&'):]
    patterns = list(dict.fromkeys(e for entries, window in variants for e in (*entries, window) if e))
    lines = ['#include "explorer/abi_patterns/pattern.h"',
             '#include "explorer/native_abi_internal.h"', '', 'namespace quickky {', 'namespace {', '',
             '// Generated by scripts/Generate-NativeAbiPatterns.py from exact test oracles.',
             '// Complete instruction templates; only listed external address operands vary.']
    for index, (code, _, references) in enumerate(patterns):
        lines.append(f'constexpr unsigned char kBytes{index}[]{{')
        for offset in range(0, len(code), 24):
            lines.append('    ' + ','.join(f'0x{x:02x}' for x in code[offset:offset + 24]) + ',')
        lines.append('};')
        if references:
            lines.append(f'constexpr NativeCodeReference kRefs{index}[]{{')
            lines.extend('    {0x%x,0x%x,0x%x,%d},' % ref for ref in references)
            lines.append('};')
    def name(pattern):
        if pattern is None:
            return '{}'
        index = patterns.index(pattern)
        code, unwind, references = pattern
        refs = f'kRefs{index}' if references else 'nullptr'
        return '{kBytes%d,0x%x,%s,%s,%d}' % (index, len(code), str(unwind).lower(), refs, len(references))
    lines.extend(['', 'constexpr NativeAbiVariant kVariants[]{'])
    for entries, window in variants:
        lines.append('    {{' + ','.join(name(e) for e in entries) + '},' + name(window) + '},')
    lines.extend(['};', '', '} // namespace', '', tail])
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research', type=Path, required=True)
    parser.add_argument('--dumpbin', type=Path, required=True)
    parser.add_argument('--image-override', action='append', default=[], metavar='VERSION=PATH')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    research = args.research.resolve()
    overrides = dict(item.split('=', 1) for item in args.image_override)
    samples = []
    for filename in ('inventory.json', 'server-returned-inventory.json'):
        samples.extend(json.loads((research / filename).read_text(encoding='utf-8'))['Samples'])
    records = {s['Version']: s for s in samples}
    sources = sorted((ROOT / 'tests/abi_reference').glob('10.*.cpp'))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(lambda s: sample(s, records[s.stem], research,
                                                args.dumpbin.resolve(), overrides), sources))
    families = {rule: [] for rule in range(1, 6)}
    for version, rule, variant in results:
        if variant not in families[rule]:
            families[rule].append(variant)
        print(version, 'family', rule, 'variant', families[rule].index(variant))
    outputs = []
    for rule, variants in families.items():
        path = ROOT / 'src/explorer/abi_patterns' / f'family_{rule}.cpp'
        outputs.append((path, emit(rule, variants, path)))
    for path, text in outputs:
        if args.check:
            if path.read_text(encoding='utf-8') != text:
                raise ValueError(f'Generated templates differ: {path}')
        else:
            path.write_text(text, encoding='utf-8', newline='\n')
    print('Normalized variants:', {rule: len(v) for rule, v in families.items()})


if __name__ == '__main__':
    main()
