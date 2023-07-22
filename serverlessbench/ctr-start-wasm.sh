#!/bin/bash

echo "执行的文件名：$0";

echo "容器wasm-calc1—>share:$1:"
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2  \
--cpu-period=$1  \
--label module.wasm.image/variant=compat \
docker.io/zhuye001x/calc:latest calc1 &

for((i = 2; i <= 32; i++ ));
do
echo "容器wasm-calc$i—>share:$2:"
sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2   \
--cpu-period=$2  \
--label module.wasm.image/variant=compat \
docker.io/zhuye001x/calc:latest calc$i &
done

wait
echo "容器启动结束！"