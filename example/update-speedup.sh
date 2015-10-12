mkdir plots
mkdir plots/update_speedup

dorun=$1

runs=$2

baseline=$3

echo $runs

#if [ $dorun -eq 1 ]
#then
#  prun speedup -runs $runs -baseline "rake-compress-update.virtual -seq ${baseline}" -parallel "rake-compress-update.virtual -seq 0 -proc 1,2,4,6,8,10,12,15,20,30,39" -type add -n 1000000 -graph binary_tree -k 1,100,1000,10000,30000,60000,100000,300000,999999 -output "plots/update_speedup/binary_tree_update_${baseline}.txt"
#fi
#cp "plots/update_speedup/binary_tree_update_${baseline}.txt" "plots/update_speedup/binary_tree_update_${baseline}.pdf"
#pplot speedup -series k -input "plots/update_speedup/binary_tree_update_${baseline}.pdf"
#cp _results/chart-1.pdf plots/update_speedup/binary_tree_update_${baseline}.pdf

#if [ $dorun -eq 1 ]
#then
#  prun speedup -runs $runs -baseline "rake-compress-update.virtual -seq ${baseline}" -parallel "rake-compress-update.virtual -seq 0 -proc 1,2,4,6,8,10,12,15,20,30,39" -type add -n 1000000 -graph bamboo -k 1,10,100,1000,10000,30000,60000,100000,300000,999999 -output "plots/update_speedup/bamboo_update_${baseline}.txt"
#fi
#cp "plots/update_speedup/bamboo_update_${baseline}.txt" "plots/update_speedup/bamboo_update_${baseline}.pdf"
#pplot speedup -series k -input "plots/update_speedup/bamboo_update_${baseline}.pdf"
#cp _results/chart-1.pdf plots/update_speedup/bamboo_update_${baseline}.pdf

#prun -runs $runs -prog rake-compress-update.opt -n 1000000 -seq 1 -type add,delete -fraction 0.1 -graph random_tree -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999

for f in 1#0 0.3 0.6 1 # 0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9
do
  if [ $dorun -eq 1 ]
  then
    prun speedup -runs $runs -baseline "rake-compress-update.virtual -seq ${baseline}" -parallel "rake-compress-update.virtual -seq 0 -proc 1,2,4,6,8,10,12,15,20,30,39" -type add -n 1000000 -graph random_tree -fraction ${f} -k 1,10,100,1000,10000,30000,60000,100000,300000,999999 -output "plots/update_speedup/random_${f}_update_${baseline}.txt"
  fi
  cp "plots/update_speedup/random_${f}_update_${baseline}.txt" "plots/update_speedup/random_${f}_update_${baseline}.pdf"
  pplot speedup -series k -input "plots/update_speedup/random_${f}_update_${baseline}.pdf"
  cp _results/chart-1.pdf plots/update_speedup/random_${f}_update_${baseline}.pdf
done
