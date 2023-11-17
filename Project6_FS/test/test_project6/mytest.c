#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];

int main(void)
{
    int i, j;
    int fd = sys_fopen("big.txt", O_RDWR);
    int pos = (1u << 20); //1M
    
    // write
    sys_lseek(fd, pos);
    sys_fwrite(fd, "hello world!\n", 13);

    sys_lseek(fd, pos);
    sys_fread(fd, buff, 13);
    for (j = 0; j < 12; j++)
    {
        printf("%c", buff[j]);
    }
    printf("offset = %d", pos);
    printf("%c", buff[j]);
    
    sys_fclose(fd);
}
