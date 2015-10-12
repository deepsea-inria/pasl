mkdir plots
mkdir plots/construction

dorun=$1

runs=$2

baseline=$3

echo $runs

if [ $dorun -eq 1 ]
then
  prun speedup -runs $runs -baseline "rake-compress-construction.virtual -seq ${baseline}" -parallel "rake-compress-construction.virtual -seq 0 -proc 1,2,4,6,8,10,12,15,20,30,39" -n 7000000 -graph binary_tree -fraction 0 -output "plots/construction/binary_tree_construction_${baseline}.txt"
fi
#cp "plots/construction/binary_tree_construction_${baseline}.txt" "plots/construction/binary_tree_construction_${baseline}.pdf"
#pplot speedup -input "plots/construction/binary_tree_construction_${baseline}.pdf"
#cp _results/chart-1.pdf plots/construction/binary_tree_construction_${baseline}.pdf

#if [ $dorun -eq 1]
#then
#  prun speedup -runs $runs -baseline "rake-compress-construction.virtual -seq ${baseline}" -parallel "rake-compress-construction.virtual -seq 0 -proc 1,2,4,6,8,10,12,15,20,30,39" -n 7000000 -graph bamboo -output "plots/construction/bamboo_construction_${baseline}.txt"
#fi
#cp "plots/construction/bamboo_construction_${baseline}.txt" "plots/construction/bamboo_construction_${baseline}.pdf"
#pplot speedup -input "plots/construction/bamboo_construction_${baseline}.pdf"
#cp _results/chart-1.pdf plots/construction/bamboo_construction_${baseline}.pdf

for f in 0 0.3 0.6 1 # 0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9
do
  if [ $dorun -eq 1 ]
  then
    prun speedup -runs $runs -baseline "rake-compress-construction.virtual -seq ${baseline}" -parallel "rake-compress-construction.virtual -seq 0 -proc 1,2,4,6,8,10,12,15,20,30,39" -n 7000000 -graph random_tree -fraction $f -output "plots/construction/random_${f}_construction_${baseline}.txt"
  fi
#  cp "plots/construction/random_${f}_construction_${baseline}.txt" "plots/construction/random_${f}_construction_${baseline}.pdf"
#  pplot speedup -input "plots/construction/random_${f}_construction_${baseline}.pdf"
#  cp _results/chart-1.pdf plots/construction/random_${f}_construction_${baseline}.pdf
done

cd "plots/construction"
cat "binary_tree_construction_${baseline}.txt" "random_0_construction_${baseline}.txt" "random_0.3_construction_${baseline}.txt" "random_0.6_construction_${baseline}.txt" "random_1_construction_${baseline}.txt" > "results.txt"
cp "results.txt" "plot_${baseline}.pdf"
pplot speedup -input "plot_${baseline}.pdf" -series graph,fraction
cp _results/chart-1.pdf "plot_${baseline}.pdf"
cd ../../
