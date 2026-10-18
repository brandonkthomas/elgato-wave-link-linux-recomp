#!/usr/bin/env python3
"""Compare RNNoise weight arrays embedded in a Mach-O slice against upstream rnn_data.c files.

usage: compare_weights.py <macho_slice> <symbols_demangled.txt> <rnn_data.c> [more rnn_data.c ...]
The slice's __TEXT segment must start at file offset 0 / vmaddr 0 (true for the plugin bundles).
"""
import re, sys, hashlib

def parse_arrays(path):
    src = open(path).read()
    out = {}
    for m in re.finditer(r'(?:static\s+)?const\s+rnn_weight\s+(\w+)\s*\[(\d+)\]\s*=\s*\{(.*?)\};', src, re.S):
        vals = [int(v) for v in re.findall(r'-?\d+', m.group(3))]
        assert len(vals) == int(m.group(2)), (m.group(1), len(vals), m.group(2))
        out[m.group(1)] = vals
    return out

def parse_syms(path):
    syms = {}
    for line in open(path):
        m = re.match(r'^([0-9a-f]{16}) [sSdD] _(\w+)$', line.strip())
        if m:
            syms[m.group(2)] = int(m.group(1), 16)
    return syms

def main():
    blob = open(sys.argv[1], 'rb').read()
    syms = parse_syms(sys.argv[2])
    refs = [(p, parse_arrays(p)) for p in sys.argv[3:]]
    names = list(refs[0][1].keys())
    for name in names:
        n = len(refs[0][1][name])
        addr = syms.get(name)
        if addr is None:
            print(f'{name}: not found in binary symbols'); continue
        raw = blob[addr:addr + n]
        vals = [b - 256 if b > 127 else b for b in raw]
        h = hashlib.sha1(bytes(raw)).hexdigest()[:12]
        res = []
        for p, arrs in refs:
            ref = arrs.get(name)
            if ref is None:
                res.append(f'{p.split("/")[-1]}=missing'); continue
            diff = sum(1 for a, b in zip(vals, ref) if a != b)
            res.append(f'{p.split("/")[-1]}={"IDENTICAL" if diff == 0 and len(ref)==n else f"{diff} diffs (len {len(ref)})"}')
        print(f'{name:32s} @0x{addr:x} n={n:6d} sha1={h} first8={vals[:8]}  ' + '  '.join(res))

if __name__ == '__main__':
    main()
