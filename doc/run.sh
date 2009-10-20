#!/bin/bash

./makelatex.pl *.doc && latex surealived_??.tex && latex surealived_??.tex
dvipdf surealived_PL.dvi
dvipdf surealived_EN.dvi
xpdf surealived_EN.pdf

