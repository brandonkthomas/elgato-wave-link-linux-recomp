#!/usr/bin/env python3
"""Tiny Mach-O (thin, 64-bit) reader: map vmaddr -> file offset and dump typed values.
usage: machoread.py <slice> <fmt> <0xaddr> [count]
fmt: f32 | f64 | i32 | u32 | u64 | ptr | cstr | bytes
"""
import struct, sys

def segments(blob):
    magic, cputype, cpusub, ftype, ncmds, sizeofcmds, flags, _ = struct.unpack_from('<IiiIIIII', blob, 0)
    assert magic == 0xfeedfacf
    off = 32; segs = []
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from('<II', blob, off)
        if cmd == 0x19:  # LC_SEGMENT_64
            segname = blob[off+8:off+24].rstrip(b'\0').decode()
            vmaddr, vmsize, fileoff, filesize = struct.unpack_from('<QQQQ', blob, off+24)
            segs.append((segname, vmaddr, vmsize, fileoff, filesize))
        off += cmdsize
    return segs

def v2o(segs, addr):
    for name, vm, vms, fo, fs in segs:
        if vm <= addr < vm + fs:
            return fo + (addr - vm)
    raise ValueError(hex(addr))

def main():
    blob = open(sys.argv[1], 'rb').read()
    segs = segments(blob)
    fmt = sys.argv[2]; addr = int(sys.argv[3], 16); n = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    o = v2o(segs, addr)
    if fmt == 'cstr':
        for i in range(n):
            e = blob.index(b'\0', o); print(hex(addr), repr(blob[o:e].decode('utf-8', 'replace'))); addr += e - o + 1; o = e + 1
        return
    if fmt == 'bytes':
        print(blob[o:o+n].hex()); return
    code, size = {'f32': ('<f', 4), 'f64': ('<d', 8), 'i32': ('<i', 4), 'u32': ('<I', 4), 'u64': ('<Q', 8), 'ptr': ('<Q', 8)}[fmt]
    for i in range(n):
        v = struct.unpack_from(code, blob, o + i * size)[0]
        if fmt == 'ptr':
            # chained fixups: low 36 bits hold the target for rebases in arm64e/arm64 DYLD_CHAINED_PTR_64(_OFFSET)
            tgt = v & 0xFFFFFFFFF
            s = ''
            try:
                to = v2o(segs, tgt); e = blob.index(b'\0', to)
                cand = blob[to:e]
                if 0 < len(cand) < 200 and all(32 <= c < 127 for c in cand): s = repr(cand.decode())
            except Exception: pass
            print(f'{hex(addr + i*size)}: raw=0x{v:x} target=0x{tgt:x} {s}')
        else:
            print(f'{hex(addr + i*size)}: {v!r}')

if __name__ == '__main__':
    main()
