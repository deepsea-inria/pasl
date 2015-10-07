mkdir plots
mkdir plots/update

dorun=$1

runs=$2

echo $runs

if [ $dorun -eq 1 ]
then
  prun -runs $runs -prog rake-compress-update.virtual -n 1000000 -seq 1 -type add,delete -graph binary_tree -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999 -output "plots/update/binary_tree_update.txt"
fi
cp "plots/update/binary_tree_update.txt" "plots/update/binary_tree_update.pdf"
pplot scatter -x k -y exectime -series type -input "plots/update/binary_tree_update.pdf"
cp _results/chart-1.pdf plots/update/binary_tree_update.pdf

if [ $dorun -eq 1 ]
then
  prun -runs $runs -prog rake-compress-update.virtual -n 1000000 -seq 1 -type add,delete -graph bamboo -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999 -output "plots/update/bamboo_update.txt"
fi
cp "plots/update/bamboo_update.txt" "plots/update/bamboo_update.pdf"
pplot scatter -x k -y exectime -series type -input "plots/update/bamboo_update.pdf"
cp _results/chart-1.pdf plots/update/bamboo_update.pdf

#prun -runs $runs -prog rake-compress-update.virtual -n 1000000 -seq 1 -type add,delete -fraction 0.1 -graph random_tree -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999

for f in 0 0.3 0.6 1 #0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9
do
  if [ $dorun -eq 1 ]
  then
    prun -runs $runs -prog rake-compress-update.virtual -n 1000000 -seq 1 -type add,delete -fraction $f -graph random_tree -k 1,10,30,100,300,1000,3000,10000,30000,100000,200000,300000,400000,500000,600000,700000,800000,900000,999999 -output "plots/update/random_${f}_update.txt"
  if
  cp "plots/update/random_${f}_update.txt" "plots/update/random_${f}_update.pdf"
  pplot scatter -x k -y exectime -series type -input "plots/update/random_${f}_update.pdf"
  cp _results/chart-1.pdf plots/update/random_${f}_update.pdf
done
