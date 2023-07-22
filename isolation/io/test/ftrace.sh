#!/bin/sh  

dir=/sys/kernel/debug/tracing
home=/home/femu/proj227-nopnop-code-repository/isolation/io/test
disk=/dev/nvme0n1
# echo "format nvme..."  
# mkfs.fat ${disk}
# echo "mount nvme to /mnt"  
# mount ${disk} /mnt/  
sysctl kernel.ftrace_enabled=1  
echo "SSD Write..."  
echo nop > ${dir}/current_tracer  
echo function_graph > ${dir}/current_tracer  
echo 1 > ${dir}/tracing_on  
# dd if=/dev/zero of=${home}/writedd.test oflag=direct bs=1K count=1024000
./ddtest.sh 
echo 0 > ${dir}/tracing_on  
# cat ${dir}/trace > ${home}/SSD_Write.txt  
# echo "SSD Read..."  
# echo nop > ${dir}/current_tracer  
# echo function_graph > ${dir}/current_tracer  
# echo 1 > ${dir}/tracing_on  
# dd if=${home}/writedd.test of=/dev/null bs=1M count=512  
# echo 0 > ${dir}/tracing_on  
# cat ${dir}/trace > ${home}/SSD_Read.txt  
# echo nop > ${dir}/current_tracer  