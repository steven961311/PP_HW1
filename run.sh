map=test
comment="test"
file=hw1.cpp

echo -e $comment >> info.txt

g++ -std=c++17 -O3 -pthread -fopenmp $file -o hw1

OMP_NUM_THREADS=6  ./hw1 samples/$map.txt > answer.txt 2>> info.txt

echo -e "" >> info.txt

python3 validate.py samples/$map.txt answer.txt
