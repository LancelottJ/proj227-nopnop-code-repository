#!/bin/bash
# 参数说明：$1-容器个数, $2-cpu.shares $3-stress-ng进程数

echo "执行的文件名：$0";

# 编译crun:具有cgroup功能
path=`pwd`
cd /home/femu/crun_disabled
sudo make install > /dev/null 2>&1
echo "crun关闭cgroup功能"
cd $path

sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu1 stress-ng --cpu 1 --timeout 600s &


sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu2 stress-ng --io 4 --timeout 600s &

wait
echo "容器启动完成!"

sleep 2s

# 获取容器进程号pid—>arr[]
declare -a arr 
arr=$(sudo ctr t ls | grep "stress" | awk '{print $2}')

for i in ${arr[@]}
do
    # 绑核
    taskset -pc 0 $i

    tpid=$(pgrep -P $i)

    for j in ${tpid[@]}
    do
        taskset -pc 0 $j
    done
done
echo "taskset执行完毕"


