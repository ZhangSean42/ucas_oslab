#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

char buff[100000];

uint16_t fletcher16(uint8_t* data, int n)
{
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;
    
    for (int i = 0; i < n; i++){
        sum1 = (sum1 + data[i]) % 0xff;
        sum2 = (sum2 + sum1) % 0xff;
    }
    return (sum2 << 8) | sum1;
}

int main()
{
    int nbytes = 20484;
    sys_net_recv_stream(buff, &nbytes);
    int size = *(int*)buff;
    memcpy(buff, buff+4, size-4);
    for (int i = 0; i < 20480; i+=5){
            if (buff[i] == 'h' && buff[i+1] == 'e' && buff[i+2] == 'l' && buff[i+3] == 'l' && buff[i+4] == 'o')
                continue;
            else{
                sys_move_cursor(0, 3);
                printf("%d\n", i);
                printf("%c%c%c%c%c\n", buff[i], buff[i+1], buff[i+2], buff[i+3], buff[i+4]);
                break;
            }
    }
    uint16_t val = fletcher16(buff, size-4);
    sys_move_cursor(0, 0);
    printf("filesz %d\n", size-4);
    printf("fletcher16 result: %d\n", val);
    return 0;    
}