#!/bin/bash

num=`expr $(sudo ctr c ls | wc -l) - 1`

for((i=$num; i >= 1; i -- ));
do
{
    sudo nerdctl rm -f stress-cpu$i
    # sudo ctr c rm stress-cpu$i
    echo "delete container stress-cpu$i successfully!"
} &
done

wait
sudo ctr c ls

