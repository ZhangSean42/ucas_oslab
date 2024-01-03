#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static char buff[4096];

int main(int argc, char* argv[])
{
    int print_location = (argc == 1) ? 0 : atoi(argv[1]);
    sys_move_cursor(0, print_location);
    int fd = sys_fopen("largefile.txt", O_RDWR);
    int iteration = 8;

    clock_t Begin = clock();
    while (iteration--){
        for (int i = 0; i < 512; i++){
        // read file_offset 0 for 128 times
            sys_lseek(fd, 0, SEEK_SET);
            sys_fread(fd, buff, 8);
        }
    }
    clock_t End = clock();
    printf("read largefile.txt finished, time cost: %d", End - Begin);
    
    sys_fclose(fd);

    return 0;
}