#!/bin/zsh

rm -f *new.f32
rm -f *.pwl
rm -f *.pfpl
rm -f *.sz3
rm -f *.zfp

python3 run-all-f32-compression-tests.py -f ./datasets/ps_data.f32   -tfrom 0.01 -tto 0.1 -tstep 0.02
python3 run-all-f32-compression-tests.py -f ./datasets/obs_spitzer.f32 -tfrom 1.0 -tto 5.0 -tstep 1.0
python3 run-all-f32-compression-tests.py -f ./datasets/obs_temp.f32    -tfrom 1.0 -tto 5.0 -tstep 1.0
