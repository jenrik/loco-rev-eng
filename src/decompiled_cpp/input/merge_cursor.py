#!/usr/bin/env python3
"""Merge the existing cursor implementation parts into one file."""
import os

base = "/home/user/projects/v43/jenrik/lego-loco-rev-eng/src/decompiled_cpp/input"

# Read the preamble (just written via Write tool)
preamble_path = os.path.join(base, "Cursor.cpp")
with open(preamble_path, "r") as f:
    preamble = f.read()

# Read the new implementations
new_path = os.path.join(base, "Cursor_new_impls.cpp")
with open(new_path, "r") as f:
    new_impls = f.read()

# Write the merged file
output_path = os.path.join(base, "Cursor_merged.cpp")
with open(output_path, "w") as f:
    f.write(preamble)
    f.write("\n\n")
    f.write(new_impls)
    f.write("\n")

print(f"Merged file written to {output_path}")
print(f"Total size: {os.path.getsize(output_path)} bytes")
