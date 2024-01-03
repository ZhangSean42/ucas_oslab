#include <tinylibdeflate.h>
#include <asm/biosdef.h>
#include <os/string.h>

#define BIOS_FUNC_ENTRY 0x50150000
#define IGNORE 0
#define compressed_kernel_start 0x53000000
#define decompressed_kernel_start 0x54000000
#define sector_byte_size 512
#define byte2sec(n) (n / sector_byte_size + (n % sector_byte_size != 0))

static long call_bios(long which, long arg0, long arg1, long arg2, long arg3, long arg4)
{
    long (*bios_func)(long,long,long,long,long,long,long,long) = \
        (long (*)(long,long,long,long,long,long,long,long))BIOS_FUNC_ENTRY;
    return bios_func(arg0, arg1, arg2, arg3, arg4, IGNORE, IGNORE, which);
}

int sd_read(unsigned mem_address, unsigned num_of_blocks, unsigned block_id)
{
    return (int)call_bios((long)BIOS_SDREAD, (long)mem_address, \
                            (long)num_of_blocks, (long)block_id, IGNORE, IGNORE);
}

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
    memcpy(0x50202000, decompressed_kernel_start, restore_nbytes);
    
    go_to_decompressed_kernel(0x50202000);
}