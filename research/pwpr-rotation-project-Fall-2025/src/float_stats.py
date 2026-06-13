import struct
import sys
from collections import defaultdict

def read_f32_le_file(path):
    """
    Read a little-endian binary file of 32-bit floats.
    Returns a list of floats.
    """
    floats = []
    with open(path, "rb") as f:
        data = f.read()
    # process 4 bytes at a time
    for i in range(0, len(data), 4):
        chunk = data[i:i+4]
        if len(chunk) < 4:
            break  # ignore incomplete final bytes
        val = struct.unpack('<f', chunk)[0]  # little-endian float
        floats.append(val)
    return floats


def main(path):
    values = read_f32_le_file(path)
    if not values:
        print("No float data found.")
        return

    # Round to 3 decimal places (the “trimmed” data)
    trimmed = [round(v, 3) for v in values]

    # Basic statistics (on raw values)
    data_min = min(values)
    data_max = max(values)
    data_avg = sum(values) / len(values)

    # Count unique trimmed values
    counts = defaultdict(int)
    for rv in trimmed:
        counts[rv] += 1

    # ---- Error analysis ----
    abs_errors = [abs(a - b) for a, b in zip(values, trimmed)]
    total_error = sum(abs_errors)
    avg_error = total_error / len(abs_errors)

    # ---- Display results ----
    print(f"File: {path}")
    print(f"Total floats read : {len(values)}")
    print(f"Data minimum      : {data_min:.6f}")
    print(f"Data maximum      : {data_max:.6f}")
    print(f"Data average      : {data_avg:.6f}")
    print(f"Unique values (rounded to 3 dp): {len(counts)}")
    print()
    print(f"Total absolute error : {total_error:.6f}")
    print(f"Average per-datum loss: {avg_error:.6f}")
    print()

    print("Value -> count")
    ## for k in sorted(counts.keys()):
    ##     print(f"{k:.3f} -> {counts[k]}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <binary_float_file>")
        sys.exit(1)
    main(sys.argv[1])
