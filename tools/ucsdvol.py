#!/usr/bin/env python3
"""ucsdvol.py -- read and write files on UCSD Pascal II.0 volume images (.BLK).

  ucsdvol.py ls   IMAGE
  ucsdvol.py get  IMAGE NAME [HOSTFILE]        (TEXT files are converted to host text)
  ucsdvol.py put  IMAGE HOSTFILE [NAME]        (NAME ending .TEXT is converted to UCSD text)
  ucsdvol.py rm   IMAGE NAME
  ucsdvol.py new  IMAGE VOLNAME [BLOCKS]       (fresh, empty volume)

Directory: blocks 2..5, 26-byte entries; entry 0 describes the volume.
TEXT files: 2-block header of zeros, then 1K pages of whole CR-terminated
lines padded with NULs; leading blanks are DLE (16) + (32+count).
"""
import struct, sys, os

BLK = 512
KIND = {0: 'untyped', 1: 'baddisk', 2: 'code', 3: 'text', 4: 'info', 5: 'data', 6: 'graf', 7: 'foto'}


class Volume:
    def __init__(self, path, data=None):
        self.path = path
        self.data = bytearray(open(path, 'rb').read() if data is None else data)
        self.read_dir()

    @classmethod
    def new(cls, name, blocks, date=0):
        """a fresh, empty volume in memory (no file: image() gives its bytes).  A sub-volume (the
        shell's MKDIR) is one written onto another as a data file: its volume name is its file
        name's first 7 characters before a '.'"""
        d = bytearray(blocks * BLK)
        vn = name.upper().split('.')[0][:7].encode('latin1')
        struct.pack_into('<HHHB', d, 2 * BLK, 0, 6, 0, len(vn))
        d[2 * BLK + 7: 2 * BLK + 7 + len(vn)] = vn
        struct.pack_into('<HHHH', d, 2 * BLK + 14, blocks, 0, 0, date)
        return cls(None, d)

    def image(self):
        """the volume's bytes, its directory written"""
        self.write_dir()
        return bytes(self.data)

    def sub(self, name):
        """the sub-volume NAME (a data file holding a volume) as a Volume in memory: put it back
        with write(NAME, sub.image(), 5)"""
        return Volume(None, self.read(name)[0])

    def used(self):
        """the blocks up to the end of the last file"""
        return max([self.dirlast] + [e[1] for e in self.entries])

    def read_dir(self):
        d = self.data[2 * BLK: 6 * BLK]
        _, self.dirlast, _, nl = struct.unpack_from('<HHHB', d, 0)
        self.volname = bytes(d[7:7 + nl]).decode('latin1')
        self.eov, n, self.loadtime, self.lastboot = struct.unpack_from('<HHHH', d, 14)
        self.entries = []
        for i in range(1, n + 1):
            e = d[26 * i: 26 * i + 26]
            first, last, kind, nl = struct.unpack_from('<HHHB', e, 0)
            name = bytes(e[7:7 + nl]).decode('latin1')
            lastbyte, date = struct.unpack_from('<HH', e, 22)
            self.entries.append([first, last, kind, name, lastbyte, date])

    def write_dir(self):
        d = bytearray(4 * BLK)
        vn = self.volname.encode('latin1')
        struct.pack_into('<HHHB', d, 0, 0, self.dirlast, 0, len(vn))
        d[7:7 + len(vn)] = vn
        struct.pack_into('<HHHH', d, 14, self.eov, len(self.entries), self.loadtime, self.lastboot)
        for i, (first, last, kind, name, lastbyte, date) in enumerate(self.entries, 1):
            nb = name.encode('latin1')
            struct.pack_into('<HHHB', d, 26 * i, first, last, kind, len(nb))
            d[26 * i + 7: 26 * i + 7 + len(nb)] = nb
            struct.pack_into('<HH', d, 26 * i + 22, lastbyte, date)
        self.data[2 * BLK: 6 * BLK] = d

    def save(self):
        self.write_dir()
        open(self.path, 'wb').write(self.data)

    def find(self, name):
        for e in self.entries:
            if e[3].upper() == name.upper():
                return e
        return None

    def read(self, name):
        e = self.find(name)
        if not e:
            raise SystemExit(f'{name}: not found on {self.volname}:')
        first, last, kind, _, lastbyte, _ = e
        raw = bytes(self.data[first * BLK: last * BLK])
        return raw[:len(raw) - BLK + lastbyte] if last > first else raw, kind & 15

    def remove(self, name):
        e = self.find(name)
        if e:
            self.entries.remove(e)

    def write(self, name, raw, kind, date=None):
        self.remove(name)
        if len(self.entries) >= 77:
            raise SystemExit('directory full')
        nblocks = max(1, (len(raw) + BLK - 1) // BLK)
        # first fit in a gap between files (entries are kept sorted by block)
        self.entries.sort(key=lambda e: e[0])
        start = self.dirlast
        pos = 0
        for pos, e in enumerate(self.entries + [[self.eov]]):
            if e[0] - start >= nblocks:
                break
            start = e[1]
        else:
            raise SystemExit('no room on volume')
        lastbyte = len(raw) - (nblocks - 1) * BLK if raw else BLK
        padded = raw + bytes(nblocks * BLK - len(raw))
        self.data[start * BLK: (start + nblocks) * BLK] = padded
        ent = [start, start + nblocks, kind, name.upper(), lastbyte, date if date is not None else self.lastboot]
        self.entries.insert(pos, ent)


def text_to_ucsd(txt):
    """host text (any line ending, tabs expanded to 8) -> UCSD TEXT file bytes"""
    out = bytearray(2 * BLK)
    page = bytearray()
    lines = txt.replace('\r\n', '\n').replace('\r', '\n').split('\n')
    if lines and lines[-1] == '':
        lines.pop()
    for ln in lines:
        ln = ln.expandtabs(8).rstrip()
        n = len(ln) - len(ln.lstrip(' '))
        b = bytearray()
        if n > 0:
            while n > 0:
                k = min(n, 223)
                b += bytes([16, 32 + k])
                n -= k
        b += ln.lstrip(' ').encode('latin1') + b'\r'
        if len(page) + len(b) > 2 * BLK - 1:
            out += page + bytes(2 * BLK - len(page))
            page = bytearray()
        page += b
    if page:
        out += page + bytes(2 * BLK - len(page))
    return bytes(out)


def ucsd_to_text(raw):
    res = []
    body = raw[2 * BLK:]
    i = 0
    line = ''
    while i < len(body):
        c = body[i]
        if c == 16 and i + 1 < len(body):
            line += ' ' * (body[i + 1] - 32)
            i += 2
            continue
        if c == 13:
            res.append(line)
            line = ''
        elif c != 0:
            line += chr(c)
        i += 1
    if line:
        res.append(line)
    return '\n'.join(res) + '\n'


def main(a):
    if len(a) < 2:
        print(__doc__)
        return 2
    cmd, img = a[0], a[1]
    if cmd == 'new':
        blocks = int(a[3]) if len(a) > 3 else os.path.getsize(img) // BLK if os.path.exists(img) else 10000
        d = bytearray(blocks * BLK)
        vn = a[2].upper().encode()
        if len(vn) > 7:
            raise SystemExit("volume name longer than 7 characters: " + a[2])
        struct.pack_into('<HHHB', d, 2 * BLK, 0, 6, 0, len(vn))
        d[2 * BLK + 7: 2 * BLK + 7 + len(vn)] = vn
        struct.pack_into('<HHHH', d, 2 * BLK + 14, blocks, 0, 0, 0)
        open(img, 'wb').write(d)
        return 0
    v = Volume(img)
    if cmd == 'ls':
        print(f'{v.volname}: ({v.eov} blocks)')
        for first, last, kind, name, lastbyte, date in sorted(v.entries, key=lambda e: e[0]):
            print(f'  {name:16s} {last - first:5d} blk  @{first:5d}  {KIND.get(kind & 15, kind)}')
    elif cmd == 'get':
        raw, kind = v.read(a[2])
        out = a[3] if len(a) > 3 else a[2]
        if kind == 3:
            open(out, 'w', newline='\n').write(ucsd_to_text(raw))
        else:
            open(out, 'wb').write(raw)
    elif cmd == 'put':
        name = (a[3] if len(a) > 3 else os.path.basename(a[2])).upper()
        if name.endswith('.TEXT'):
            v.write(name, text_to_ucsd(open(a[2], encoding='latin1').read()), 3)
        elif name.endswith('.CODE'):
            v.write(name, open(a[2], 'rb').read(), 2)
        else:
            v.write(name, open(a[2], 'rb').read(), 5)
        v.save()
    elif cmd == 'rm':
        v.remove(a[2])
        v.save()
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
