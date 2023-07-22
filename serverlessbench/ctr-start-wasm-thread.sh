#!/bin/bash

start_time=`date +%s`  #定义脚本运行的开始时间

tmp_fifofile="/tmp/$$.fifo"
mkfifo $tmp_fifofile   # 新建一个FIFO类型的文件
exec 6<>$tmp_fifofile  # 将FD6指向FIFO类型
rm $tmp_fifofile  #删也可以，

thread_num=30  # 定义最大线程数

#根据线程总数量设置令牌个数
#事实上就是在fd6中放置了$thread_num个回车符
for ((i=0;i<${thread_num};i++));do
    echo
done >&6

for((i = 1; i <= 32; i ++ ));
do
    # 一个read -u6命令执行一次，就从FD6中减去一个回车符，然后向下执行
    # 当FD6中没有回车符时，就停止，从而实现线程数量控制
    read -u6
    {
        sudo ctr run -d --rm --runc-binary crun --runtime io.containerd.runc.v2   \
        --cpu-period=$1  \
        --label module.wasm.image/variant=compat \
        docker.io/zhuye001x/calc:latest calc$i
        echo >&6 # 当进程结束以后，再向FD6中加上一个回车符，即补上了read -u6减去的那个
    } &
done

wait # 要有wait，等待所有线程结束

stop_time=`date +%s` # 定义脚本运行的结束时间
echo "TIME:`expr $stop_time - $start_time`" # 输出脚本运行时间

exec 6>&- # 关闭FD6
echo "over" # 表示脚本运行结束