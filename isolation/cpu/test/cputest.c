#include <stdio.h>
 #include <unistd.h>
int main()
{
    long long int a = 0;
    for (int i = 0; i < 100; i++)
    {
        for (int j = 0; j < 500000000; j++)
        {
            a += j;
        }
        sleep(5);
    }
    return 0;
}