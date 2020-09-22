n=200
dim=500
batch=20
calc=100
sprse=0.01

for i in `seq 1 ${n}`
do
    echo "Running Test No. $i/$n"
    python libtest.py $batch $dim $sprse $calc
    rm threadpool_test
    make
    ./threadpool_test >> test_record_${batch}_${dim}_${sprse/./-}_${calc}.log
done
