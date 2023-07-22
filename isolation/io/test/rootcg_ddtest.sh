echo "8:0 1048576" > /sys/fs/cgroup/blkio/blkio.throttle.write_bps_device
dd if=/dev/zero of=/home/femu/proj227-nopnop-code-repository/isolation/io/test/dd.test oflag=direct bs=1024 count=1000000 &
pid=$(pgrep -n dd)
echo $pid > /sys/fs/cgroup/blkio/tasks
# echo -n > /sys/fs/cgroup/blkio/foo/tasks