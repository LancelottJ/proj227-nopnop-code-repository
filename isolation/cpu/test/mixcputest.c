#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
int main()
{
    int n = 10000000;
    char buf[] = "I love OS\n";
    int fd = open("./iotest", O_RDWR);
    if (fd < 0)
    {
        printf("open error: %s\n", strerror(errno));
        return 0;
    }
    while (n--)
    {
        int ret = write(fd, buf, sizeof(buf));
        if (ret < 0)
        {
            perror("write:");
        }
    }
    return 0;
}