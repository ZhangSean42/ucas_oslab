int main(int argc, char *argv[])
{
    for (long p = 0xf10000000; p < 0xf1004f000; p += 0x1000){
        *((long*)p) = p;
    }
    for (long p = 0xf10000000; p < 0xf1004f000; p += 0x1000){
       if (*((long*)p) != p){
           sys_move_cursor(0, 0);
           printf("swap failed!\n");
           return 0;
       }
    }
    sys_move_cursor(0, 0);
    printf("swap passed!\n");
    return 0;
}