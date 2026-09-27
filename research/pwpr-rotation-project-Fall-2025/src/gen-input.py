import math
import struct

N = 50000
filename = "input_data.bin"

with open(filename, "wb") as f:
    for i in range(N):
        # compute sine sample from 0 to 2π (not inclusive)
        angle = (2 * math.pi * i) / N
        value = math.sin(angle)
        # write as little-endian 32-bit float
        f.write(struct.pack('<f', value))

print(f"Wrote {N} floats ({N * 4} bytes) to {filename}")
