#!/bin/bash
# 参数说明：$1 输出文件名


echo > temp.txt
top -n 1 -b  | grep stress-ng | awk '{print $1, $9}' >> temp.txt

declare -a arr 
arr=$(sudo ctr t ls | grep "stress" | awk '{print $2}')

declare -a tpid
var_cpu=0
cnt=0
for i in ${arr[@]}
do
    cnt=`echo $cnt+1|bc`
    pid_cost=0
    tpid=$(pgrep -P $i)
    for j in ${tpid[@]}
    do
        str=$(cat temp.txt | grep $j | awk '{print $2}')
        pid_cost=`echo $str+$pid_cost|bc`
    done
    pid_cost=`echo $pid_cost-25.0|bc`
    echo "i=$i,pid_cost=$pid_cost"
    pid_cost=`echo $pid_cost*$pid_cost|bc`
    # echo $pid_cost
    var_cpu=`echo $var_cpu+$pid_cost|bc`
done
# echo "平方和为$var_cpu"
var_cpu=`echo $var_cpu/$cnt|bc`
# echo "数量为$cnt"
echo "方差为$var_cpu"

