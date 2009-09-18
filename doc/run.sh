#!/bin/bash

./makelatex.pl *.doc && latex surealived_PL.tex && latex surealived_PL.tex
dvipdf surealived_PL.dvi
xpdf surealived_PL.pdf

