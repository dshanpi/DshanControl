"""Synthetic ELF metadata for packaging tests, never executable board evidence."""
import struct


def media_elf(cedar=True, machine=40, flags=0x5000200):
    symbols = ['TPlayerCreate', 'TPlayerDestroy', 'TPlayerSetDataSource',
               'TPlayerPrepareAsync', 'TPlayerStart', 'TPlayerStop',
               'TPlayerSetDisplayRect', 'TPlayerSetNotifyCallback'] if cedar else []
    libraries = ['libtplayer.so', 'libcdx_base.so'] if cedar else ['libc.so.6']
    strings = bytearray(b'\0'); offsets = {}
    for name in libraries + symbols:
        offsets[name] = len(strings); strings += name.encode() + b'\0'
    sections = bytearray(b'\0.dynstr\0.dynsym\0.dynamic\0.shstrtab\0.text\0')
    data = bytearray(52 + 2 * 32)
    def put(value):
        while len(data) % 4: data.append(0)
        at = len(data); data.extend(value); return at
    text = put(bytes.fromhex('1eff2fe1'))  # bx lr; fixture is not a functioning player
    ds = put(strings)
    syms = bytes(16) + b''.join(struct.pack('<IIIBBH', offsets[n], 0, 0, 0x12, 0, 0) for n in symbols)
    sym = put(syms)
    entries = [(1, offsets[n]) for n in libraries] + [(5, 0x10000+ds),
               (10, len(strings)), (6, 0x10000+sym), (11, 16), (0, 0)]
    dynamic = b''.join(struct.pack('<II', *e) for e in entries); dyn = put(dynamic)
    shstr = put(sections)
    while len(data) % 4: data.append(0)
    shoff = len(data)
    def section(name, typ, attributes, at, size, link=0, info=0, align=1, entry=0):
        return struct.pack('<IIIIIIIIII', sections.index(name.encode()+b'\0'), typ,
                           attributes, 0x10000+at if attributes else 0, at, size,
                           link, info, align, entry)
    data += bytes(40)
    data += section('.dynstr', 3, 2, ds, len(strings))
    data += section('.dynsym', 11, 2, sym, len(syms), 1, 1, 4, 16)
    data += section('.dynamic', 6, 2, dyn, len(dynamic), 1, 0, 4, 8)
    data += section('.shstrtab', 3, 0, shstr, len(sections))
    data += section('.text', 1, 6, text, 4, align=4)
    ident = b'\x7fELF\x01\x01\x01' + bytes(9)
    data[:52] = struct.pack('<16sHHIIIIIHHHHHH', ident, 2, machine, 1,
                            0x10000+text, 52, shoff, flags, 52, 32, 2, 40, 6, 4)
    data[52:84] = struct.pack('<IIIIIIII', 1, 0, 0x10000, 0x10000, len(data), len(data), 5, 4)
    data[84:116] = struct.pack('<IIIIIIII', 2, dyn, 0x10000+dyn, 0x10000+dyn,
                             len(dynamic), len(dynamic), 4, 4)
    return bytes(data)
