#!/usr/bin/env python3
"""
Lego Loco .dat tile descriptor parser
Reads the ASCII text format and returns a structured tile definition.

Each .dat file in resource.RFD describes one tile type used in the game world.
All files are ASCII text, CRLF line endings, Windows backslash paths.

Usage:
    python3 parse_dat.py <path-to-file.dat>
    python3 parse_dat.py --extract-all <art-res-dir>

Direct C equivalent: CTileDesc_ParseDat() in src/game/game_world.c
"""

import sys
import re
import os
import struct
from dataclasses import dataclass, field
from typing import List, Optional, Tuple


@dataclass
class AnimFrameSet:
    name: str           # e.g. "cursor", "W", "idle"
    set_idx: int        # animation set index
    first_frame: int    # first frame of animation
    speed: int          # frames per step (0 = static)
    delay_ms: int       # delay before playing
    sound_id: int       # sound effect ID (-1 = none)
    loop: int           # loop count (0 = infinite, -1 = no loop)
    extras: List[int]   # additional undocumented fields


@dataclass
class TileDescriptor:
    filename: str

    # Physical grid footprint
    phys_cols: int = 1
    phys_rows: int = 1
    phys_layers: int = 1
    phys_grid: List[List[List[int]]] = field(default_factory=list)

    # Visual bitmap footprint
    bmp_cols: int = 1
    bmp_rows: int = 1
    bmp_grid: List[List[int]] = field(default_factory=list)

    # Track/road connectivity: pixel offset per side (N, E, S, W)
    entry_exit: Optional[Tuple[int, int, int, int]] = None

    # Right-mouse-button animation sequence
    rmb_seq: int = -1

    # Minifig behavior
    leisure_dest: int = 0
    free_to_roam: Optional[Tuple[int, int, int, int]] = None  # x1,y1,x2,y2
    max_employees: int = 0
    possible_employees: List[int] = field(default_factory=list)
    max_minifig: int = 0
    possible_minifigs: List[int] = field(default_factory=list)

    # Vehicle-specific
    walk_speed: Optional[Tuple[int, int]] = None
    sex: Optional[str] = None
    pickup_sound_id: int = -1
    ground_width: int = 0

    # Display
    shifts: Optional[Tuple[int, int, int, int]] = None
    button_visible: int = 1
    closed_fs: int = -1
    button_offset: Optional[Tuple] = None
    hotspot: Optional[Tuple[int, int]] = None

    # Animation
    total_frames: int = 1
    num_frame_sets: int = 1
    cursor_frame_set: Optional[Tuple[int, int]] = None
    frame_sets: List[AnimFrameSet] = field(default_factory=list)

    # Sequence triggers
    insert_seq: Optional[Tuple[int, int]] = None
    insert_easter_egg: Optional[str] = None
    mobile_seq: Optional[Tuple[int, int]] = None
    mobile_easter_egg: Optional[str] = None


def parse_dat(content: str, filename: str = '') -> TileDescriptor:
    td = TileDescriptor(filename=filename)

    # Normalize line endings
    lines = content.replace('\r\n', '\n').replace('\r', '\n').split('\n')

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        # Skip empty lines, comments, separators
        if not line or line.startswith('//') or line == '-9':
            i += 1
            continue

        tok = line.split()
        key = tok[0].lower() if tok else ''

        # ----------------------------------------------------------------
        # physical_occupancy
        # ----------------------------------------------------------------
        if key == 'physical_occupancy':
            i += 1
            # Blank line separator
            while i < len(lines) and not lines[i].strip():
                i += 1
            # Dimensions line: cols rows [layers]
            dim_line = lines[i].strip().split()
            td.phys_cols = int(dim_line[0])
            td.phys_rows = int(dim_line[1])
            td.phys_layers = int(dim_line[2]) if len(dim_line) > 2 else 1
            i += 1
            td.phys_grid = []
            for layer in range(td.phys_layers):
                layer_grid = []
                rows_read = 0
                while rows_read < td.phys_rows and i < len(lines):
                    row_line = lines[i].strip()
                    i += 1
                    if not row_line:
                        continue
                    row_vals = [int(v) for v in row_line.split()]
                    if row_vals:
                        layer_grid.append(row_vals)
                        rows_read += 1
                td.phys_grid.append(layer_grid)
            continue

        # ----------------------------------------------------------------
        # bitmap_occupancy
        # ----------------------------------------------------------------
        elif key == 'bitmap_occupancy':
            i += 1
            while i < len(lines) and not lines[i].strip():
                i += 1
            dim_line = lines[i].strip().split()
            td.bmp_cols = int(dim_line[0])
            td.bmp_rows = int(dim_line[1])
            i += 1
            td.bmp_grid = []
            rows_read = 0
            while rows_read < td.bmp_rows and i < len(lines):
                row_line = lines[i].strip()
                i += 1
                if not row_line or row_line.startswith('//'):
                    continue
                row_vals = [int(v) for v in row_line.split()]
                if row_vals:
                    td.bmp_grid.append(row_vals)
                    rows_read += 1
            continue

        # ----------------------------------------------------------------
        # Simple key-value fields
        # ----------------------------------------------------------------
        elif key == 'entry_exit' and len(tok) >= 5:
            td.entry_exit = (int(tok[1]), int(tok[2]), int(tok[3]), int(tok[4]))

        elif key == 'rmbseq' and len(tok) >= 2:
            td.rmb_seq = int(tok[1])

        elif key == 'leisuredestination' and len(tok) >= 2:
            td.leisure_dest = int(tok[1])

        elif key == 'freetroam' and len(tok) >= 5:
            td.free_to_roam = (int(tok[1]), int(tok[2]), int(tok[3]), int(tok[4]))

        elif key == 'maxemployees' and len(tok) >= 2:
            td.max_employees = int(tok[1])

        elif key == 'possibleemployees':
            td.possible_employees = [int(v) for v in tok[1:]]

        elif key == 'maxminifigforresource' and len(tok) >= 2:
            td.max_minifig = int(tok[1])

        elif key == 'possibleminifigs':
            td.possible_minifigs = [int(v) for v in tok[1:]]

        elif key == 'walk_speed' and len(tok) >= 3:
            td.walk_speed = (int(tok[1]), int(tok[2]))

        elif key == 'sex' and len(tok) >= 2:
            td.sex = tok[1]

        elif key == 'pickupsoundid' and len(tok) >= 2:
            td.pickup_sound_id = int(tok[1])

        elif key == 'groundwidth' and len(tok) >= 2:
            td.ground_width = int(tok[1])

        elif key == 'shifts' and len(tok) >= 5:
            td.shifts = (int(tok[1]), int(tok[2]), int(tok[3]), int(tok[4]))

        elif key == 'buttonvisible' and len(tok) >= 2:
            td.button_visible = int(tok[1])

        elif key == 'closedfs' and len(tok) >= 2:
            td.closed_fs = int(tok[1])

        elif key == 'button' and len(tok) >= 2 and tok[1].lower() == 'offset':
            td.button_offset = tuple(int(v) for v in tok[2:])

        elif key == 'hotspot' and len(tok) >= 3:
            td.hotspot = (int(tok[1]), int(tok[2]))

        elif key == 'total_number_of_frames' and len(tok) >= 2:
            td.total_frames = int(tok[1])

        elif key == 'number_of_frame_sets' and len(tok) >= 2:
            td.num_frame_sets = int(tok[1])

        elif key == 'cursor/default_frame_set' and len(tok) >= 3:
            td.cursor_frame_set = (int(tok[1]), int(tok[2]))

        elif key == 'cursor_frame_set' and len(tok) >= 3:
            td.cursor_frame_set = (int(tok[1]), int(tok[2]))

        elif key == 'insertseq' and len(tok) >= 3:
            td.insert_seq = (int(tok[1]), int(tok[2]))

        elif key == 'mobileseq' and len(tok) >= 3:
            td.mobile_seq = (int(tok[1]), int(tok[2]))

        elif key == 'easteregg':
            # Just store as raw string for now
            prev_key = ''
            if td.insert_seq is not None and td.insert_easter_egg is None:
                td.insert_easter_egg = line
            else:
                td.mobile_easter_egg = line

        # ----------------------------------------------------------------
        # Animation frame set table (named frame sets after -9 sentinel)
        # These are lines like: W   0  0  1 0  0 -1 0 0 0  0
        # ----------------------------------------------------------------
        elif len(tok) >= 2 and td.num_frame_sets > 0:
            try:
                nums = [int(v) for v in tok[1:]]
                if len(nums) >= 2:
                    fs = AnimFrameSet(
                        name=tok[0],
                        set_idx=nums[0] if len(nums) > 0 else 0,
                        first_frame=nums[1] if len(nums) > 1 else 0,
                        speed=nums[2] if len(nums) > 2 else 1,
                        delay_ms=nums[4] if len(nums) > 4 else 0,
                        sound_id=nums[5] if len(nums) > 5 else -1,
                        loop=nums[8] if len(nums) > 8 else 0,
                        extras=nums[9:] if len(nums) > 9 else [],
                    )
                    td.frame_sets.append(fs)
            except (ValueError, IndexError):
                pass

        i += 1

    return td


def print_tile(td: TileDescriptor):
    print(f"Tile: {td.filename}")
    print(f"  Physical:  {td.phys_cols}w x {td.phys_rows}d x {td.phys_layers}h layers")
    print(f"  Bitmap:    {td.bmp_cols}w x {td.bmp_rows}h")
    if td.entry_exit:
        n, e, s, w = td.entry_exit
        print(f"  Connections: N={n} E={e} S={s} W={w}")
    if td.rmb_seq != -1:
        print(f"  RMB sequence: {td.rmb_seq}")
    if td.max_employees > 0:
        print(f"  Employees: max={td.max_employees} types={td.possible_employees}")
    if td.max_minifig > 0:
        print(f"  Visitors:  max={td.max_minifig} types={td.possible_minifigs}")
    if td.shifts:
        print(f"  Shifts: {td.shifts}")
    print(f"  Animation: {td.total_frames} frames, {td.num_frame_sets} sets")
    if td.frame_sets:
        print(f"  Frame sets ({len(td.frame_sets)}):")
        for fs in td.frame_sets[:5]:
            print(f"    [{fs.set_idx}] {fs.name:10s}: first={fs.first_frame} speed={fs.speed} snd={fs.sound_id}")
        if len(td.frame_sets) > 5:
            print(f"    ... and {len(td.frame_sets)-5} more")


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='Parse Lego Loco .dat tile descriptors')
    parser.add_argument('file', nargs='?', help='.dat file to parse')
    parser.add_argument('--extract-all', metavar='ART_RES_DIR',
                        help='Parse all .dat files from resource.RFD and print summary')
    args = parser.parse_args()

    if args.extract_all:
        # Parse all .dat files from the archive
        rfh_path = os.path.join(args.extract_all, 'resource.RFH')
        rfd_path = os.path.join(args.extract_all, 'resource.RFD')

        with open(rfh_path, 'rb') as f:
            rfh_data = f.read()

        offset = 0
        rfd_offset = 0
        entries = {}
        while offset < len(rfh_data) - 4:
            name_len = struct.unpack_from('<I', rfh_data, offset)[0]
            if name_len == 0 or name_len > 500:
                break
            offset += 4
            filename = rfh_data[offset:offset+name_len-1].decode('ascii', errors='replace')
            offset += name_len
            rfd_size, flags = struct.unpack_from('<II', rfh_data, offset)
            offset += 8
            entries[filename] = (rfd_offset, rfd_size, flags)
            rfd_offset += rfd_size

        dat_entries = {k: v for k, v in entries.items() if k.endswith('.dat')}
        print(f"Found {len(dat_entries)} .dat files")

        with open(rfd_path, 'rb') as rfd:
            for name, (off, size, flags) in list(dat_entries.items())[:20]:
                rfd.seek(off)
                content = rfd.read(size).decode('ascii', errors='replace')
                td = parse_dat(content, name)
                print_tile(td)
                print()

    elif args.file:
        with open(args.file, 'r', encoding='ascii', errors='replace') as f:
            content = f.read()
        td = parse_dat(content, args.file)
        print_tile(td)
    else:
        parser.print_help()
