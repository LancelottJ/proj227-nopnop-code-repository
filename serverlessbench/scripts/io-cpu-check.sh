#!/bin/bash
# 参数说明：

sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu2 stress-ng --io 1  --timeout 600s 

echo > container-io.txt
top -n 1 -b  | grep stress-ng | awk '{print $1}' >> container-io.txt

declare -a arr 
arr=$(cat container-io.txt)

for i in ${arr[@]}
do
    taskset -pc 1 $i
    tpid=$(pgrep -P $i)
    for j in ${tpid[@]}
    do
        taskset -pc 1 $j
    done
done

echo "绑核完成"