# cd /sys/fs/cgroup/blkio/
mkdir /sys/fs/cgroup/blkio/foo
echo "8:0 1048576" > /sys/fs/cgroup/blkio/foo/blkio.throttle.write_bps_device