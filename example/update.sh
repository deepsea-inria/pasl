mkdir plots
mkdir plots/update

dorun=$1

runs=$2

echo $runs

if [ $dorun -eq 1 ]
then
  prun -runs $runs -prog rake-compress-update.virtual -n 1000000 -seq 1 -type add -graph binary_tree -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999 -fraction 0 -output plots/update/binary_tree_update_add.txt
  prun -runs $runs -prog rake-compress-update.virtual -n 1000000 -seq 1 -type delete -graph binary_tree -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999 -fraction 0 -output plots/update/binary_tree_update_delete.txt
fi
#cp "plots/update/binary_tree_update.txt" "plots/update/binary_tree_update.pdf"
#pplot scatter -x k -y exectime -series type -input "plots/update/binary_tree_update.pdf"
#cp _results/chart-1.pdf plots/update/binary_tree_update.pdf
#
#if [ $dorun -eq 1 ]
#then
#  prun -runs $runs -prog rake-compress-update.virtual -n 1000000 -seq 1 -type add,delete -graph bamboo -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999 -output "plots/update/bamboo_update.txt"
#fi
#cp "plots/update/bamboo_update.txt" "plots/update/bamboo_update.pdf"
#pplot scatter -x k -y exectime -series type -input "plots/update/bamboo_update.pdf"
#cp _results/chart-1.pdf plots/update/bamboo_update.pdf

#prun -runs $runs -prog rake-compress-update.virtual -n 1000000 -seq 1 -type add,delete -fraction 0.1 -graph random_tree -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999

for f in 0 0.3 0.6 1
do
  if [ $dorun -eq 1 ]
  then
    prun -runs $runs -prog rake-compress-update.virtual -n 1000000 -seq 1 -type add -fraction $f -graph random_tree -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999 -output "plots/update/random_${f}_update_add.txt"
    prun -runs $runs -prog rake-compress-update.virtual -n 1000000 -seq 1 -type delete -fraction $f -graph random_tree  -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999 -output "plots/update/random_${f}_update_delete.txt"
  fi
#  cp "plots/update/random_${f}_update.txt" "plots/update/random_${f}_update.pdf"
#  pplot scatter -x k -y exectime -series type -input "plots/update/random_${f}_update.pdf"
#  cp _results/chart-1.pdf plots/update/random_${f}_update.pdf
done

cd "plots/update"
cat binary_tree_update_add.txt "random_0_update_add.txt" "random_0.3_update_add.txt" "random_0.6_update_add.txt" "random_1_update_add.txt" > "results_add.txt"
cp "results_add.txt" "plot_add.pdf"
pplot scatter -input "plot_add.pdf" -x k -y exectime -series graph,fraction
cp _results/chart-1.pdf "plot_add.pdf"

cat binary_tree_update_delete.txt "random_0_update_delete.txt" "random_0.3_update_delete.txt" "random_0.6_update_delete.txt" "random_1_update_delete.txt" > "results_delete.txt"
cp "results_delete.txt" "plot_delete.pdf"
pplot scatter -input "plot_delete.pdf" -x k -y exectime -series graph,fraction
cp _results/chart-1.pdf "plot_delete.pdf"

cd ../../