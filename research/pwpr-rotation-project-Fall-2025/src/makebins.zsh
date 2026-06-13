#!/bin/zsh

# BUILD pwlr --> Piecewise Linear Regression
cp pwlr old-pwlr
g++ -std=c++17 -O2 pwlrCompDecomp.cpp -o pwlr
cp pwlr /opt/homebrew/bin 

# BUILD pwpr --> Piecewise Polynomial Regression
cp pwpr old-pwpr
g++ -std=c++17 -O2 pwprCompDecomp.cpp -o pwpr
cp pwpr /opt/homebrew/bin

