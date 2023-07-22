#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

int main()
{
    char str[] = {"./test1"};
    char buf[1024];
    int n = 1000, i = 0;
    int arr[1024] = {0};
    while (n--)
    {

        int fd = open(str, O_RDWR);
        if (fd <= 0)
            perror("open");
        int ret = read(fd, buf, sizeof(buf));
        char *temp = strtok(buf, " ");
        while (temp)
        {
            printf("%s\n", temp);
            temp = strtok(NULL, " ");
        }
        if (ret < 0)
            perror("read");
        close(fd);
        // system("pgrep -P 1");
    }

    for (int j = 0; j < i - 1; j++)
    {

        printf("%d\n", arr[j]);
    }
    // int ret = execv(str[0], &str[1]);
    // if (ret)
    //     perror("exec");
    printf("1233\n");
    return 0;
}