#!/bin/bash

for((i = 1; i <= 32; i ++ ));
do
{
    sudo nerdctl stop calc$i
    sudo ctr c rm calc$i
    echo "delete wasm-container calc$i successfully!"
} &
done

wait
sudo ctr c ls