#!/bin/bash

PAPER=$HOME/Work/deepsea/2015/sc-submit-graphs/_results/

make graph.pbench

#./graph.pbench overview -size large -proc 40 -only plot -sub main
#cp _results/chart-1.pdf $PAPER/main.pdf

./graph.pbench overview -size large -proc 40 -only plot -sub cong
cp _results/chart-1.pdf $PAPER/cong.pdf

./graph.pbench overview -size large -proc 40 -only plot -sub speedup
cp _results/chart-1.pdf $PAPER/bfs.pdf

./graph.pbench overview -size large -proc 40 -only plot -sub ligra
cp _results/chart-1.pdf $PAPER/ligra.pdf

./graph.pbench overview -size large -proc 40 -only plot -sub graphs
cp _results/latex.tex $PAPER/table_graphs.tex

./graph.pbench overview -size large -only plot -proc 40 -sub locality
cp _results/chart-1.pdf $PAPER/locality.pdf
