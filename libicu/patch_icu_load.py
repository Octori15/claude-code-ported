#!/usr/bin/env python3
"""Make a Mach-O binary load libicushim.dylib in place of /usr/lib/libicucore.A.dylib.

usage: patch_icu_load.py FILE [NEW_INSTALL_NAME]

Rewrites, in place, the path in the load command that names
/usr/lib/libicucore.A.dylib (default new path: @loader_path/libicushim.dylib,
so libicushim.dylib must sit in the same directory as FILE). The command keeps
its size, so no other byte of the file moves. Thin and universal 64-bit
binaries are both handled; a file that is already patched is left as it is.
The edit invalidates the code signature, so re-sign afterwards:
    codesign --force --sign - FILE
"""
import struct
import sys

OLD = b"/usr/lib/libicucore.A.dylib"
DEFAULT_NEW = b"@loader_path/libicushim.dylib"

MH_MAGIC_64 = 0xFEEDFACF
FAT_MAGIC = 0xCAFEBABE
FAT_MAGIC_64 = 0xCAFEBABF
# LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_REEXPORT_DYLIB, LC_LAZY_LOAD_DYLIB, LC_LOAD_UPWARD_DYLIB
DYLIB_COMMANDS = (0xC, 0x80000018, 0x8000001F, 0x20, 0x80000023)


class PatchError(Exception):
    pass


def slice_offsets(f):
    """File offset of each Mach-O slice (one for a thin binary)."""
    f.seek(0)
    magic, = struct.unpack(">I", f.read(4))
    if magic not in (FAT_MAGIC, FAT_MAGIC_64):
        return [0]
    count, = struct.unpack(">I", f.read(4))
    offsets = []
    for _ in range(count):
        if magic == FAT_MAGIC:
            _cpu, _sub, offset, _size, _align = struct.unpack(">iiIII", f.read(20))
        else:
            _cpu, _sub, offset, _size, _align, _res = struct.unpack(">iiQQII", f.read(32))
        offsets.append(offset)
    return offsets


def patch_slice(f, base, new):
    f.seek(base)
    header = f.read(32)
    if len(header) < 32 or struct.unpack_from("<I", header)[0] != MH_MAGIC_64:
        raise PatchError("no 64-bit Mach-O header at offset %d" % base)
    ncmds, sizeofcmds = struct.unpack_from("<II", header, 16)
    commands = f.read(sizeofcmds)
    if len(commands) < sizeofcmds:
        raise PatchError("truncated load commands at offset %d" % base)

    result = None
    pos = 0
    for _ in range(ncmds):
        cmd, size = struct.unpack_from("<II", commands, pos)
        if size < 8 or pos + size > sizeofcmds:
            raise PatchError("malformed load command at offset %d" % (base + 32 + pos))
        if cmd in DYLIB_COMMANDS:
            name_offset, = struct.unpack_from("<I", commands, pos + 8)
            room = size - name_offset  # bytes available for the path and its NUL
            name = commands[pos + name_offset:pos + size].split(b"\0", 1)[0]
            if name == OLD:
                if len(new) + 1 > room:
                    raise PatchError("%s is longer than the %d bytes available"
                                     % (new.decode(), room - 1))
                f.seek(base + 32 + pos + name_offset)
                f.write(new + b"\0" * (room - len(new)))
                result = "patched"
            elif name == new and result is None:
                result = "already patched"
        pos += size
    if result is None:
        raise PatchError("no load command for %s" % OLD.decode())
    return result


def main(argv):
    if len(argv) not in (2, 3):
        sys.stderr.write("usage: patch_icu_load.py FILE [NEW_INSTALL_NAME]\n")
        return 2
    path = argv[1]
    new = argv[2].encode() if len(argv) == 3 else DEFAULT_NEW
    try:
        with open(path, "r+b") as f:
            results = [patch_slice(f, base, new) for base in slice_offsets(f)]
    except (OSError, PatchError, struct.error) as e:
        sys.stderr.write("patch_icu_load.py: %s: %s\n" % (path, e))
        return 1
    if sys.stderr.isatty():  # quiet when another program reads stderr
        sys.stderr.write("%s: %s\n" % (path, ", ".join(results)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
