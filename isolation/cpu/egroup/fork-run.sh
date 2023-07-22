#!/bin/bash

# enter the .../isolation/cpu/egroup dir and exec.
MYPATH=/home/femu/proj227-nopnop-code-repository/isolation/cpu
TESTELF_PATH=${MYPATH}/test/containertest.out
MSGQELF_PATH=${MYPATH}/egroup/msgcrun
CGROUP_PATH=/sys/fs/cgroup/cpu,cpuacct
OUTPUT=${MYPATH}/egroup/output

SHARES=4096
DEFAULTSHARES=1024

# gcc ${MYPATH}/test/cputest.c -o ${TESTELF_PATH}

declare -a pid_arr
# clear output
echo -n > ${OUTPUT}

for ((i=1;i<=$1;i++))
do
    ${TESTELF_PATH} &
    #echo $(pgrep -P $! | wc -l)

    if [ $i -le 1 ];
    then
        echo "group $i shares: ${SHARES}"
        echo "${SHARES} $(pgrep -P $! | wc -l)" >> ${OUTPUT}
    else
        echo "group $i shares: ${DEFAULTSHARES}"
        echo "${DEFAULTSHARES} $(pgrep -P $! | wc -l)">> ${OUTPUT}
    fi

    echo "container $! create group $i"

    # get and write all child process
    
    echo $(pgrep -P $!) >> ${OUTPUT}


done

# ${MSGQELF_PATH} ${pid_arr[@]}



# for i in ${pid_arr[@]}
# do
#     echo $i >
# done

${MSGQELF_PATH}