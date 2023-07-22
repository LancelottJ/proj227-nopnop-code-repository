#!/bin/bash
# 参数说明：$1 容器个数

# 获取top数据
echo > cpu_temp.txt
top -n 1 -b  | grep stress-ng-cpu | awk '{print $1, $9}' >> cpu_temp.txt

# 获取容器进程号pid—>arr[]
declare -a arr 
arr=$(sudo ctr t ls | grep "stress" | awk '{print $2}')

# echo > cpu_proportion.txt

printf "%-45s%-45s\n" "container pid" "cpu proportion" > cpu_proportion.txt

declare -a tpid
cnt=0

low=$((3200/$1/2))
high=$((3200/$1+5))

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
    # echo "pid_cost:$pid_cost"
    # 输出cpu占用率
    # if(($pid_cost_s>$low && $pid_cost_s<$high))
    # then
        str=$(for i in `seq 1 $(($pid_cost_s))`; do printf '■'; done)
        printf "%-20s\033[1;37m[%-`expr 40 + $(($pid_cost_s*2))`s]$pid_cost%%\n" $i $str

        # 将cpu占用率输出到cpu_proportion.txt
        echo $i $pid_cost >> cpu_proportion.txt
    # fi
done

echo "完成,cnt=$cnt"