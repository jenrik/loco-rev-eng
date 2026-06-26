#!/usr/bin/env python3
"""
Lego Loco Resource Extractor
Extracts all files from resource.RFH + resource.RFD into a directory tree.

Usage:
    python3 extract_resources.py [art-res-dir] [output-dir]

Defaults:
    art-res-dir = ../lego-loco-unpacked/art-res/
    output-dir  = ./extracted/
"""

import struct
import os
import sys
import argparse

def parse_rfh(rfh_path):
    """Parse the .RFH index file.

    RFH format (per entry, little-endian):
      uint32  name_len    length of filename including null terminator
      char[]  filename    null-terminated, backslash-separated path
      uint32  rfd_size    byte size of the asset in the .RFD
      uint32  flags       0x00 = normal, 0x01 = packed/special

    RFD offsets are computed by accumulating rfd_size values.
    """
    with open(rfh_path, 'rb') as f:
        data = f.read()

    entries = []
    offset = 0
    rfd_offset = 0

    while offset < len(data) - 4:
        name_len = struct.unpack_from('<I', data, offset)[0]
        if name_len == 0 or name_len > 512:
            break
        offset += 4

        filename = data[offset:offset + name_len - 1].decode('ascii', errors='replace')
        offset += name_len

        if offset + 8 > len(data):
            break
        rfd_size, flags = struct.unpack_from('<II', data, offset)
        offset += 8

        # Normalize path separators for Linux
        linux_path = filename.replace('\\', '/').lower()

        entries.append({
            'name':       filename,
            'path':       linux_path,
            'rfd_offset': rfd_offset,
            'rfd_size':   rfd_size,
            'flags':      flags,
        })
        rfd_offset += rfd_size

    return entries


def extract_all(rfh_path, rfd_path, output_dir):
    """Extract all assets from the .RFD file into output_dir."""
    print(f"Parsing {rfh_path}...")
    entries = parse_rfh(rfh_path)
    print(f"Found {len(entries)} entries in index.")

    total_bytes = sum(e['rfd_size'] for e in entries)
    print(f"Total data: {total_bytes:,} bytes ({total_bytes / 1024 / 1024:.1f} MB)")

    os.makedirs(output_dir, exist_ok=True)

    with open(rfd_path, 'rb') as rfd:
        for i, entry in enumerate(entries):
            out_path = os.path.join(output_dir, entry['path'])
            os.makedirs(os.path.dirname(out_path), exist_ok=True)

            rfd.seek(entry['rfd_offset'])
            asset_data = rfd.read(entry['rfd_size'])

            with open(out_path, 'wb') as out:
                out.write(asset_data)

            if (i + 1) % 250 == 0 or i == len(entries) - 1:
                print(f"  [{i+1:4d}/{len(entries)}] {entry['path']}")

    print(f"\nExtracted to: {output_dir}")

    # Summary by type
    from collections import Counter
    exts = Counter(
        e['path'].rsplit('.', 1)[-1] if '.' in e['path'] else '(none)'
        for e in entries
    )
    print("\nFile type breakdown:")
    for ext, count in sorted(exts.items(), key=lambda x: -x[1]):
        print(f"  .{ext:<10s} {count:4d} files")

    return entries


def print_dat_info(entries, output_dir):
    """Print analysis of .dat tile descriptor files."""
    dat_entries = [e for e in entries if e['path'].endswith('.dat')]
    print(f"\n=== .dat tile descriptor files ({len(dat_entries)} total) ===")

    seen_keys = {}
    for e in dat_entries[:5]:
        path = os.path.join(output_dir, e['path'])
        try:
            with open(path, 'r', encoding='ascii', errors='replace') as f:
                content = f.read()
            # Find all section headers (non-empty lines that aren't numbers)
            for line in content.split('\n')[:30]:
                line = line.strip()
                if line and not line.startswith('//') and not all(
                    c in '0123456789 .-\t' for c in line
                ):
                    seen_keys[line.split()[0]] = True
        except Exception:
            pass

    print("Common .dat keys found:", list(seen_keys.keys())[:15])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Lego Loco resource extractor')
    parser.add_argument('art_res', nargs='?',
                        default='../lego-loco-unpacked/art-res/',
                        help='Path to art-res directory')
    parser.add_argument('output', nargs='?',
                        default='./extracted/',
                        help='Output directory')
    args = parser.parse_args()

    rfh = os.path.join(args.art_res, 'resource.RFH')
    rfd = os.path.join(args.art_res, 'resource.RFD')

    if not os.path.exists(rfh):
        print(f"Error: {rfh} not found", file=sys.stderr)
        sys.exit(1)
    if not os.path.exists(rfd):
        print(f"Error: {rfd} not found", file=sys.stderr)
        sys.exit(1)

    entries = extract_all(rfh, rfd, args.output)
    print_dat_info(entries, args.output)
