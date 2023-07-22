cat /sys/kernel/debug/tracing/trace_pipe |grep 'throtl_qnode_add_bio\|blk_throtl_dispatch_work_fn'


dir=/sys/kernel/debug/tracing
home=/home/femu/proj227-nopnop-code-repository/isolation/io/test
disk=/dev/nvme0n1
sysctl kernel.ftrace_enabled=1  
echo "SSD Write..."  
echo nop > /sys/kernel/debug/tracing/current_tracer  
echo function_graph > /sys/kernel/debug/tracing/current_tracer  
echo 1 > /sys/kernel/debug/tracing/tracing_on  
# dd if=/dev/zero of=${home}/writedd.test oflag=direct bs=1K count=1024000
./ddtest.sh 
echo 0 > /sys/kernel/debug/tracing/tracing_on  