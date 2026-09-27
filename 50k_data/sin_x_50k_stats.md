# Dataset Statistics

## Dataset

| Item | Value |
|------|------:|
| Expression | `sin(x*pi/180)` |
| Samples | 50000 |
| x start | 0 |
| x step | 1 |
| Output file | `../50k_data/sin_x_50k.f32` |
| Data type | float32 |
| Byte order | Little Endian |
| Noise tolerance | ±0.000000% |
| Random seed | None |

## X Value Statistics

| Statistic | Value |
|-----------|------:|
| Count | 50000 |
| Finite | 50000 |
| Minimum | 0 |
| Maximum | 49999 |
| Mean | 24999.5 |
| Std Dev | 14433.7568 |
| First | 0 |
| Last | 49999 |
## Base Expression Statistics

| Statistic | Value |
|-----------|------:|
| Count | 50000 |
| Finite | 50000 |
| Minimum | -1 |
| Maximum | 1 |
| Mean | 0.000274511578 |
| Std Dev | 0.707303226 |
| First | 0 |
| Last | -0.656059027 |
## Output Dataset Statistics

| Statistic | Value |
|-----------|------:|
| Count | 50000 |
| Finite | 50000 |
| Minimum | -1 |
| Maximum | 1 |
| Mean | 0.000274511578 |
| Std Dev | 0.707303226 |
| First | 0 |
| Last | -0.656059027 |
## Preview

| Sample | x | Base | Output | Noise | Noise % |
|------:|--:|-----:|-------:|------:|--------:|
| 0 | 0 | 0 | 0 | 0 | 0.000000% |
| 1 | 1 | 0.0174524058 | 0.0174524058 | 0 | 0.000000% |
| 2 | 2 | 0.0348994955 | 0.0348994955 | 0 | 0.000000% |
| 3 | 3 | 0.0523359552 | 0.0523359552 | 0 | 0.000000% |
| 4 | 4 | 0.0697564706 | 0.0697564706 | 0 | 0.000000% |
| 5 | 5 | 0.0871557444 | 0.0871557444 | 0 | 0.000000% |
| 6 | 6 | 0.104528464 | 0.104528464 | 0 | 0.000000% |
| 7 | 7 | 0.121869341 | 0.121869341 | 0 | 0.000000% |
| 8 | 8 | 0.139173105 | 0.139173105 | 0 | 0.000000% |
| 9 | 9 | 0.156434461 | 0.156434461 | 0 | 0.000000% |

## Generation Command

```text
scripts/gen_le_f32_floats.py sin(x*pi/180) 50000 ../50k_data/sin_x_50k.f32 --xstart 0 --xstep 1 --plot
```
