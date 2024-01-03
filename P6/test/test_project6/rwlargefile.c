#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];

int main(void)
{
    int fd = sys_fopen("largefile.txt", O_RDWR);
    sys_move_cursor(0, 0);

    // write 'hello world!' to file_offset 0
    sys_lseek(fd, 0, SEEK_SET);
    sys_fwrite(fd, "hello world from 0!\n", 20);
    
    // write 'hello world!' to file_offset 2MiB
    sys_lseek(fd, (1 << 21), SEEK_SET);
    sys_fwrite(fd, "hello world from 2MiB!\n", 23);
    
    // write 'hello world!' to file_offset 8MiB
    sys_lseek(fd, (1 << 23), SEEK_SET);
    sys_fwrite(fd, "hello world from 8MiB!\n", 23);

    // read file_offset 0
    sys_lseek(fd, 0, SEEK_SET);
    sys_fread(fd, buff, 20);
    for (int j = 0; j < 20; j++)
    {
        printf("%c", buff[j]);
    }
        
    // read file_offset 2MiB
    sys_lseek(fd, (1 << 21), SEEK_SET);
    sys_fread(fd, buff, 23);
    for (int j = 0; j < 23; j++)
    {
        printf("%c", buff[j]);
    }
    
    // read file_offset 8MiB
    sys_lseek(fd, (1 << 23), SEEK_SET);
    sys_fread(fd, buff, 23);
    for (int j = 0; j < 23; j++)
    {
        printf("%c", buff[j]);
    }

    sys_fclose(fd);

    return 0;
}