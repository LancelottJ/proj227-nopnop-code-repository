declare -a arr
# while :
# do 
#     # arr=($(pgrep -f stress-ng))
#     arr=$(ps -A| grep "stress-ng" | grep -v "ctr" | awk '{print $1}')
#     for i in ${arr[@]}
#     do
    
#         taskset -pc 0 $i
#     done
# done 


# while :
# do 
#     # arr=($(pgrep -f stress-ng))
#     arr=$(ps -A| grep "stress-ng" | grep -v "ctr" | awk '{print $1}')
#     for i in ${arr[@]}
#     do
    
#         taskset -pc 0 $i
#     done
# done

arr=$(ps -A| grep "stress-ng" | grep -v "ctr" | awk '{print $1}')
for i in ${arr[@]}
    do
    
        taskset -pc '0-31' $i
    done