#!/bin/bash

declare -a arr
declare -a child_arr

int_handler()
{
    echo "Interrupted."
    # echo ${arr[@]}
    for i in ${arr[@]}
    do

        child_arr=($(pgrep -P $i))
        for j in ${child_arr[@]}
        do
            kill $j
        done
    done
    exit 1
}
trap 'int_handler' INT


/home/femu/proj227-nopnop-code-repository/isolation/cpu/test/containertest.o &
echo "1st task: "$!
arr[${#arr[@]}]=$!
/home/femu/proj227-nopnop-code-repository/isolation/cpu/test/containertest.o &
echo "2st task: "$!
arr[${#arr[@]}]=$!
/home/femu/proj227-nopnop-code-repository/isolation/cpu/test/containertest.sleep.o &
echo "3nd task: "$!
arr[${#arr[@]}]=$!
wait