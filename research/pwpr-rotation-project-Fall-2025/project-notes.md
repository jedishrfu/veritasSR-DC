# PWLR Tests with Dr Burtscher’s FP Datasets

{{TOC}}

## The FP Datasets

<u>**Scientific IEEE 754 64-Bit Double-Precision Floating-Point Datasets**</u>

https://userweb.cs.txstate.edu/~burtscher/research/datasets/FPdouble/

By downloading one or more datasets, you agree:

- not to distribute any of them without acknowledging the source (i.e., this web page), and
- ==to acknowledge the source and to cite at least one of the following papers wherever you publish results derived from these datasets.==

M. Burtscher and P. Ratanaworabhan. ==“High Throughput Compression of Double-Precision Floating-Point Data.”== 2007 Data Compression Conference, pp. 293-302. March 2007. [pdf]

M. Burtscher and P. Ratanaworabhan. ==“FPC: A High-Speed Compressor for Double-Precision Floating-Point Data.”== IEEE Transactions on Computers, Vol. 58, No. 1, pp. 18-31. January 2009. [pdf]

---

I chose these two datasets for my project:

- ==obs_spitzer (195 MB)==: data from the Spitzer Space Telescope showing a slight darkening as an extrasolar planet disappears behind its star
- ==obs_temp (40 MB)==: data from a weather satellite denoting how much the observed temperature differs from the actual contiguous analysis temperature field

## Future Directions

- Explore these and the other datasets with PWPR, collecting data by epsilon and by polynomial degree.
- Compare degree segments as a spectrum of the data

---

## COMMAND: ./pwl ps_data.bin 0.025 (good CR // good EPS)
- Input file: ps_data.bin  (Processing Test File)
- Epsilon: 0.025
- Read 50000 samples (200.00 KB)
- ==Compression produced 1487 segments== in 6 ms
- Wrote segments to 'segments.bin' (35.69 KB)
- Also wrote human-readable segments to 'segments.txt'
- ==Compression ratio (raw/compressed): 5.604:1==
- Decompression rebuilt 50000 samples in 0 ms
- Wrote reconstructed samples to 'reconstructed.bin' (200.00 KB)
- ==MAE vs original: 0.008115 (good EPS > MAE)==

## COMMAND: ./pwlr obs_temp.bin 0.01 (poor CR // poor EPS)
- Input file: obs_temp.bin
- Epsilon: 0.01
- Read 4991784 samples (19.97 MB)
- ==Compression produced 2485055 segments== in 47 ms
- Wrote segments to 'segments.bin' (59.64 MB)
- Also wrote human-readable segments to 'segments.txt'
- ==Compression ratio (raw/compressed): 0.335:1==
- Decompression rebuilt 4991784 samples in 7 ms
- Wrote reconstructed samples to 'reconstructed.bin' (19.97 MB)
- ==MAE vs original: 0.142081 (poor EPS < MAE)==

## COMMAND:  ./pwlr obs_temp.bin 0.1 (poor CR // poor EPS)
- Input file: obs_temp.bin
- Epsilon: 0.1
- Read 4991784 samples (19.97 MB)
- ==Compression produced 2389081 segments== in 57 ms
- Wrote segments to 'segments.bin' (57.34 MB)
- Also wrote human-readable segments to 'segments.txt'
- ==Compression ratio (raw/compressed): 0.348:1==
- Decompression rebuilt 4991784 samples in 6 ms
- Wrote reconstructed samples to 'reconstructed.bin' (19.97 MB)
- ==MAE vs original: 0.143540 (poor EPS < MAE)==

## COMMAND:  ./pwlr obs_temp.bin 1.0 (poor CR // good EPS)
- Input file: obs_temp.bin
- Epsilon: 1
- Read 4991784 samples (19.97 MB)
- ==Compression produced 1420353 segments== in 82 ms
- Wrote segments to 'segments.bin' (34.09 MB)
- Also wrote human-readable segments to 'segments.txt'
- ==Compression ratio (raw/compressed): 0.586:1==
- Decompression rebuilt 4991784 samples in 8 ms
- Wrote reconstructed samples to 'reconstructed.bin' (19.97 MB)
- ==MAE vs original: 0.381897 (good EPS > MAE)==

---
## ==OBS_TEMP Transition point between epsilon 2 and 3==

---


## COMMAND: ./pwlr obs_temp.bin 5.0 (good CR // good EPS)
- Input file: obs_temp.bin
- Epsilon: 5
- Read 4991784 samples (19.97 MB)
- ==Compression produced 165713 segments== in 1827 ms
- Wrote segments to 'segments.bin' (3.98 MB)
- Also wrote human-readable segments to 'segments.txt'
- ==Compression ratio (raw/compressed): 5.021:1==
- Decompression rebuilt 4991784 samples in 3 ms
- Wrote reconstructed samples to 'reconstructed.bin' (19.97 MB)
- ==MAE vs original: 1.276547 (good EPS > MAE)==

## COMMAND:  ./pwlr obs_temp.bin 10.0 (good CR // good EPS)
- Input file: obs_temp.bin
- Epsilon: 10
- Read 4991784 samples (19.97 MB)
- ==Compression produced 24741 segments== in 29284 ms
- Wrote segments to 'segments.bin' (593.78 KB)
- Also wrote human-readable segments to 'segments.txt'
- ==Compression ratio (raw/compressed): 33.627:1==
- Decompression rebuilt 4991784 samples in 2 ms
- Wrote reconstructed samples to 'reconstructed.bin' (19.97 MB)
- ==MAE vs original: 1.515460 (good EPS>  MAE)==

---

