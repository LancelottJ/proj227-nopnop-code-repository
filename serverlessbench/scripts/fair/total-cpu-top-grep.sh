#!/bin/bash
# 参数说明：$1 容器个数

# 获取top数据
echo > cpu_temp.txt
top -n 1 -b  | grep stress-ng | awk '{print $1, $9}' >> cpu_temp.txt

# 获取容器进程号pid—>arr[]
declare -a arr 
arr=$(sudo ctr t ls | grep "stress" | awk '{print $2}')

declare -a tpid
cnt=0

# 遍历计算容器进程
for i in ${arr[@]}
do  
    cnt=`echo $cnt+1|bc`
    # echo -n "pid=$i"
    pid_cost=0
    tpid=$(pgrep -P $i)
    for j in ${tpid[@]}
    do
        str=$(cat cpu_temp.txt | grep $j | awk '{print $2}')
        pid_cost=`echo $str+$pid_cost|bc`
    done

    pid_cost_s=${pid_cost%.*}

    # 输出cpu占用率
    if [ `echo "$pid_cost > 25"|bc` -eq 1 ];
    then
        str=$(for i in `seq 1 $((25))`; do printf '■'; done)
        str_more=$(for i in `seq 1 $(($pid_cost_s-25))`; do printf '■'; done)
        printf "%-20s\033[1;32m $pid_cost%% %-s\033[0m" $i $str
        printf "\033[1;31m%s\n\033[0m" $str_more
    elif [ `echo "$pid_cost <= 25"|bc` -eq 1 ];
    then
        str=$(for i in `seq 1 $(($pid_cost_s))`; do printf '■'; done)
        str_less=$(for i in `seq 1 $((25-$pid_cost_s))`; do printf '■'; done)
        printf "%-20s\033[1;32m $pid_cost%% %-s\033[0m" $i $str
        printf "\033[1;31m%s\n\033[0m" $str_less
    fi
done

