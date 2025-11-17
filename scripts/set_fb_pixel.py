#!/usr/bin/env python3
import sys
x, y = map(int, sys.argv[1:3])
color = int(sys.argv[3], 16)
path = '.bin/casseos.img'
print(f"Would set pixel at ({x},{y}) to color {color:#08x}")
