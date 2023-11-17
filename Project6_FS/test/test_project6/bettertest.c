#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define GAP   (1u << 20)
//包含0~7M的空间。

static char buff[64];

int main(void)
{
    int i, j;
    int fd = sys_fopen("big.txt", O_RDWR);
    int pos = 0;
    
    // write
    for(i = 0; i < 8; i++)
    {
        sys_lseek(fd, pos);
        sys_fwrite(fd, "hello world!\n", 13);
        pos += GAP;
    }

    for(i = 0, pos = 0; i < 8; i++)
    {
        sys_lseek(fd, pos);
        sys_fread(fd, buff, 13);
        for (j = 0; j < 12; j++)
        {
            printf("%c", buff[j]);
        }
        printf("offset = %d", pos);
        printf("%c", buff[j]);
        pos += GAP;
    }

    sys_fclose(fd);
}
