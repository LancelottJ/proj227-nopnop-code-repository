#!/bin/bash

# enter the .../isolation/cpu/egroup dir and exec.
MYPATH=/home/femu/proj227-nopnop-code-repository/isolation/cpu
TESTELF_PATH=${MYPATH}/test/containertest.out
MSGQELF_PATH=${MYPATH}/egroup/msgcrun
CGROUP_PATH=/sys/fs/cgroup/cpu,cpuacct

SHARES=4096
DEFAULTSHARES=1024

# gcc ${MYPATH}/test/cputest.c -o ${TESTELF_PATH}

declare -a pid_arr
# clear
echo -n > input

for ((i=1;i<=$1;i++))
do
    ${TESTELF_PATH} &

    sudo mkdir $CGROUP_PATH/$i
    echo "container $! create group $i"
    
    if [ $i -le 1 ];
    then
        sudo echo ${SHARES} > ${CGROUP_PATH}/$i/cpu.shares
        echo "group $i shares: ${SHARES}"
    else
        # overwrite last shares
        sudo echo ${DEFAULTSHARES} > ${CGROUP_PATH}/$i/cpu.shares
        echo "group $i shares: ${DEFAULTSHARES} "
    fi

    # get all child process
    pid_arr=($(pgrep -P $!))

    # write tasks equal to write procs
    # sudo echo $! > ${CGROUP_PATH}/$i/cgroup.procs
    for j in ${pid_arr[@]}
    do
        echo $j >> ${CGROUP_PATH}/$i/tasks
        echo "group $i:$j"
    done

    
    # 每个组一行？
    echo ${pid_arr[@]} >> input
done

# ${MSGQELF_PATH} ${pid_arr[@]}



# for i in ${pid_arr[@]}
# do
#     echo $i >
# done

# ${MSGQELF_PATH}