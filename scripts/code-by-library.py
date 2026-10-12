#!/usr/bin/env python3
"""Attributes a Mach-O slice's __text to the libraries it was built from (scripts/check.sh size).

usage: code-by-library.py <binary> <arch> <index dir>...

A shipping (LTO) link puts every function in one object, so its linker map attributes nothing. The
symbols keep their names, so each one is looked up in the same sources' non-LTO objects (the index
directories, usually the normal build's CMakeFiles folders and the CLAP library), which say which
JUCE module, codec or eq1 library defined it. A symbol in no object (a template instance or a
function LTO renamed) is placed by the namespace in its demangled name instead.

The size of a symbol is the gap to the next one in __text (the last one reaches the section's end),
so the buckets add up to the section. Prints one line per bucket, bytes first, largest first, then
a total, with the bytes of __text itself for comparison.
"""
import os
import re
import subprocess
import sys
from collections import Counter


def run(*args, stdin=None):
    return subprocess.run(args, input=stdin, check=True, capture_output=True, text=True, errors="replace").stdout


def bucket_of_object(path):
    """The library an object file belongs to, from its path in the build folder."""
    name = os.path.basename(path)
    unix = path.replace("\\", "/")
    if "clap" in unix.lower():
        return "CLAP (clap-juce-extensions)"
    if "/eq1_engine.dir/" in unix:
        return "eq1 Engine"
    module = re.search(r"/modules/(juce_[a-z0-9_]+)/", unix)
    if module is None:
        return "eq1 Plugin Shell"
    module = module.group(1)
    for marker, bucket in (
        ("_Harfbuzz", "harfbuzz"),
        ("_Sheenbidi", "sheenbidi"),
        ("_libjpg", "codec: libjpg"),
        ("_libpng", "codec: libpng"),
        ("_libwebp", "codec: libwebp"),
        ("_lunasvg", "lunasvg"),
        ("_flac", "codec: flac"),
        ("_ogg", "codec: ogg"),
        ("_opus", "codec: opus"),
        ("_zlib", "codec: zlib"),
    ):
        if marker in name:
            return bucket
    return module


def symbol_index(arch, index_dirs):
    """Symbol name -> the buckets of the objects defining it, from every object file and static library
    under the index directories. A template instance is defined by every object that used it."""
    files = []
    for directory in index_dirs:
        if os.path.isfile(directory):
            files.append(directory)
            continue
        for root, _, names in os.walk(directory):
            files.extend(os.path.join(root, n) for n in names if n.endswith((".o", ".a")))
    index = {}
    for i in range(0, len(files), 64):
        try:
            out = run("nm", "-A", "-arch", arch, "--defined-only", *files[i : i + 64])
        except subprocess.CalledProcessError as error:
            out = error.stdout
        for line in out.splitlines():
            # <file>[:<member>]: <address> <type> <name>
            head, _, rest = line.rpartition(": ")
            parts = rest.split()
            if len(parts) != 3 or head == "":
                continue
            path = head.split(":")[0]
            index.setdefault(parts[2], set()).add(bucket_of_object(path))
    return index


def text_symbols(binary, arch):
    """(address, name) of every symbol in __text, by address, and the section's end."""
    section = run("otool", "-l", "-arch", arch, binary)
    match = re.search(r"sectname __text\n\s+segname __TEXT\n\s+addr (0x[0-9a-f]+)\n\s+size (0x[0-9a-f]+)", section)
    if match is None:
        sys.exit(f"{binary}: no __text section for {arch}")
    end = int(match.group(1), 16) + int(match.group(2), 16)
    symbols = []
    for line in run("nm", "-n", "-m", "-arch", arch, "--defined-only", binary).splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[1] == "(__TEXT,__text)":
            symbols.append((int(parts[0], 16), parts[-1]))
    return symbols, end, int(match.group(2), 16)


def bucket_by_name(demangled):
    """Where a symbol goes when no object, or more than one, defines it: by its namespace."""
    demangled = re.sub(r"^(non-virtual |virtual |covariant return )?thunk to ", "", demangled)
    if demangled.startswith("___cxx_global_var_init") or demangled.startswith("_GLOBAL__sub_I_"):
        return "static initialisers"
    for prefix, bucket in (
        ("eq1::", "eq1 templates (by namespace)"),
        ("staple::", "eq1 templates (by namespace)"),
        ("juce::", "JUCE templates (by namespace)"),
        ("std::", "libc++ templates"),
        ("hb_", "harfbuzz"),
        ("_hb_", "harfbuzz"),
        ("OT::", "harfbuzz"),
        ("AAT::", "harfbuzz"),
        ("CFF::", "harfbuzz"),
        ("Steinberg::", "VST3 SDK"),
        ("clap", "CLAP (clap-juce-extensions)"),
        ("_WebP", "codec: libwebp"),
        ("_VP8", "codec: libwebp"),
        ("_png_", "codec: libpng"),
        ("_jpeg_", "codec: libjpg"),
        ("_FLAC", "codec: flac"),
        ("_ogg_", "codec: ogg"),
        ("_vorbis", "codec: ogg"),
        ("_opus_", "codec: opus"),
        ("-[", "Objective-C"),
        ("+[", "Objective-C"),
    ):
        if demangled.startswith(prefix):
            return bucket
    # A template instance whose name starts with its return type: "void std::sort<juce::...>(...)".
    owner = re.match(r"^[\w\s:<>,*&]*?\b(eq1|staple|juce|std)::", demangled)
    if owner is not None:
        return {"eq1": "eq1 templates (by namespace)", "staple": "eq1 templates (by namespace)",
                "juce": "JUCE templates (by namespace)", "std": "libc++ templates"}[owner.group(1)]
    return "other"


def lookup(index, name):
    """The one bucket defining the symbol, trying it with LTO's numeric suffixes (".123") taken off;
    None when no object or several define it."""
    while True:
        if name in index:
            buckets = index[name]
            return next(iter(buckets)) if len(buckets) == 1 else None
        stripped = re.sub(r"\.\d+$", "", name)
        if stripped == name:
            return None
        name = stripped


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    binary, arch, index_dirs = sys.argv[1], sys.argv[2], sys.argv[3:]
    index = symbol_index(arch, index_dirs)
    symbols, end, text_size = text_symbols(binary, arch)
    demangled = run("c++filt", stdin="\n".join(name for _, name in symbols) + "\n").splitlines()
    sizes = Counter()
    for i, (address, name) in enumerate(symbols):
        next_address = symbols[i + 1][0] if i + 1 < len(symbols) else end
        bucket = lookup(index, name)
        if bucket is None:
            bucket = bucket_by_name(demangled[i] if i < len(demangled) else name)
        sizes[bucket] += next_address - address
    width = len(f"{text_size:,}")
    for bucket, size in sizes.most_common():
        if size > 0:
                print(f"{size:>{width},}  {bucket}")
    print(f"{sum(sizes.values()):>{width},}  total of {len(symbols)} symbols (__text is {text_size:,} bytes)")


if __name__ == "__main__":
    main()
