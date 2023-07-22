#!/bin/bash

echo "执行的文件名：$0";

# 编译crun:具有cgroup功能
path=`pwd`
cd /home/femu/crun_disabled
sudo make install > /dev/null 2>&1
echo "crun close cgroup."
cd $path

# startTime=`date +%Y%m%d-%H:%M:%S`

for((i = 1; i <= $1; i++ ));
do
echo "container stress-cpu$i—>share:$2:"
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2  \
--cpu-period=$2 \
docker.io/lorel/docker-stress-ng:latest stress-cpu$i stress-ng --cpu 1 --timeout 30s &
done

wait

# endTime=`date +%Y%m%d-%H:%M:%S`

echo "容器启动结束！"

# sumTime=$[ $endTime - $startTime ]
# echo "Total:$sumTime seconds"

