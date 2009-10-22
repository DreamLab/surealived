#!/bin/bash

./makelatex.pl *.doc && latex surealived_PL.tex && latex surealived_PL.tex
latex surealived_EN.tex && latex surealived_EN.tex
dvipdf surealived_PL.dvi
dvipdf surealived_EN.dvi
xpdf surealived_EN.pdf &
xpdf surealived_PL.pdf

