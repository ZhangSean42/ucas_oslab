#include <tinylibdeflate.h>
#include <common.h>
#include <os/string.h>

#define compressed_kernel_start 0x53000000
#define decompressed_kernel_start 0x54000000
#define sector_byte_size 512
#define byte2sec(n) (n / sector_byte_size + (n % sector_byte_size != 0))

void go_to_decompressed_kernel(unsigned addr)
{
    asm volatile(
    "jr %0"
    :
    :"r"(addr));
}

void main(){

    unsigned  compressed_kernel_byte_sz   = (unsigned)(*((uint32_t*)0x502001f0));
    unsigned  compressed_kernel_block_num = (unsigned)byte2sec(compressed_kernel_byte_sz);
    unsigned  compressed_kernel_block_id  = (unsigned)byte2sec(*((uint16_t*)0x502001fc) + 1);
    
    sd_read(compressed_kernel_start, compressed_kernel_block_num, compressed_kernel_block_id);
    
    deflate_set_memory_allocator(NULL, NULL);
    struct libdeflate_decompressor * decompressor = deflate_alloc_decompressor();
    
    int restore_nbytes = 0;
    deflate_deflate_decompress(decompressor, compressed_kernel_start, compressed_kernel_byte_sz, decompressed_kernel_start, 0x01000000, &restore_nbytes);
    memcpy(0x50201000, decompressed_kernel_start, restore_nbytes);
    
    go_to_decompressed_kernel(0x50201000);
}