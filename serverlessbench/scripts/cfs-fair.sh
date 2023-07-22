#!/bin/bash
# 参数说明：$1-容器个数/4, $2-cpu.shares

echo "执行的文件名：$0";

for((i = 1; i <= $1; i++ ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu 1 --timeout 60s &
done

for((i = $1+1; i <= 2*$1; i++ ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu 1 --timeout 60s &
done

for((i = 2*$1+1; i <= 3*$1; i++ ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu 4 --timeout 60s &
done

for((i = 3*$1+1; i <= 4*$1; i++ ));
do
echo "容器stress-ng$i—>share:1024:" &
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2 \
    --cpu-period=1024  \
    docker.io/lorel/docker-stress-ng:latest \
    stress-cpu$i stress-ng --cpu 4 --timeout 60s &    
done

wait
echo "容器启动完成!"

