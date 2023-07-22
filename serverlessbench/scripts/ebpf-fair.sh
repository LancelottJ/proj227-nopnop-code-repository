#!/bin/bash
# 参数说明：$1-容器个数, $2-cpu.shares $3-stress-ng进程数

echo "执行的文件名：$0";

# 编译crun:具有cgroup功能
path=`pwd`
cd /home/femu/crun_disabled
sudo make install > /dev/null 2>&1
echo "crun关闭cgroup功能"
cd $path

startTime_s=`date +%s`

for((i = 1; i <= $1/4; i++ ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu 1 --timeout 600s &
done

for((i = $1/4+1; i <= 2*($1/4); i++ ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu 1 --timeout 600s &
done

for((i = 2*$1/4+1; i <= 3*($1/4); i++ ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu $3 --timeout 600s &
done

for((i = 3*$1/4+1; i <= 4*($1/4) - ($1/4)/8; i++ ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu $3 --timeout 600s &    
done

wait
echo "容器启动完成!"

endTime_s=`date +%s`

sumTime=$[ $endTime_s - $startTime_s ]
echo "Total:$sumTime seconds"

sleep 2s

# echo > container.txt
# top -n 1 -b  | grep stress-ng | awk '{print $1}' >> container.txt

# 获取容器进程号pid—>arr[]
declare -a arr 
arr=$(sudo ctr t ls | grep "stress" | awk '{print $2}')

cnt=0
base=31
for i in ${arr[@]}
do
    # 绑核
    # echo "pid:$i"
    knum=`expr $cnt % $base`
    cnt=`expr $cnt + 1`
    
    taskset -pc $knum $i > /dev/null 2>&1

    tpid=$(pgrep -P $i)

    for j in ${tpid[@]}
    do
        # printf "    tpid:$j"
        taskset -pc $knum $j > /dev/null 2>&1
    done
    printf "\n"
done
echo "taskset执行完毕"


