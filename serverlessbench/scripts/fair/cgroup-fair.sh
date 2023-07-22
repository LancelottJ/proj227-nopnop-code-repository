#!/bin/bash
# 参数说明：$1-容器个数, $2-cpu.shares

echo "执行的文件名：$0";

# 编译crun:具有cgroup功能
path=`pwd`
cd /home/femu/crun
sudo make install > /dev/null 2>&1
echo "crun开启cgroup功能"
cd $path


for((i = 1; i <= $1/4; i++ ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    --cgroup "" \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu 1 --timeout 600s &
done

for((i = $1/4+1; i <= 2*($1/4); i++  ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    --cgroup "" \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu 1 --timeout 600s &
done

for((i = 2*$1/4+1; i <= 3*($1/4); i++ ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    --cgroup "" \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu 4 --timeout 600s &
done

for((i = 3*$1/4+1; i <= 4*($1/4) - ($1/4)/8; i++  ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    --cgroup "" \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu 4 --timeout 600s &    
done

wait
echo "容器启动完成!"

endTime_s=`date +%s`

sumTime=$[ $endTime_s - $startTime_s ]
echo "Total:$sumTime seconds"

sleep 2s

echo > container.txt
top -n 1 -b  | grep stress-ng | awk '{print $1}' >> container.txt

declare -a arr
arr=$(cat container.txt)

for i in ${arr[@]}
do
    taskset -pc '0-30' $i

    tpid=$(pgrep -P $i)
    for j in ${tpid[@]}
    do
        taskset -pc '0-30' $j
    done
done
