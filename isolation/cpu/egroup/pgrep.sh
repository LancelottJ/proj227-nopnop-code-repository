for ((i=1;i<=$1;i++))
do
    # pgrep -P $2 > /dev/null
    cat /proc/1/task/1/children > /dev/null
done