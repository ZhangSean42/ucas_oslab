#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tinylibdeflate.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    uint8_t  app_name[32];
    uint32_t app_img_offs;
    uint32_t app_sz;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

#define MAXSIZE 0x10000
char buff[MAXSIZE];
char compress_buff[MAXSIZE];

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void create_image_vm(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_compressed_kernel(char* compress_buff, int out_nbytes_kernel, FILE *image, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, int app_info_offs, task_info_t *taskinfo, short tasknum, FILE *img);
static void write_img_info_vm(int nbytes_kernel, int nbytes_decompress, int app_info_offs, task_info_t *taskinfo, short tasknum, FILE *img);

int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    /*if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }*/
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    if (options.extended == 1)
        create_image(argc - 1, argv + 1);
    else
        create_image_vm(argc - 1, argv + 1);
    return 0;
}

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 2;
    int nbytes_kernel = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 2;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);
        printf("%s starts from 0x%04lx\n", *files, phyaddr);
        
        /*copy app name  and img offset into taskinfo*/
        if (taskidx >= 0){
            memcpy(taskinfo[taskidx].app_name, *files, sizeof(*files));
            taskinfo[taskidx].app_name[sizeof(*files)] = 0;
            taskinfo[taskidx].app_img_offs = phyaddr;
        }

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            if (phdr.p_type != PT_LOAD) continue;

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);

            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }
        }

        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */
        /*if (strcmp(*files, "main") == 0)
            write_padding(img, &phyaddr, SECTOR_SIZE + NBYTES2SEC(nbytes_kernel) * SECTOR_SIZE);
        else if (taskidx >= 0) 
            write_padding(img, &phyaddr, SECTOR_SIZE + (taskidx+2) * NBYTES2SEC(nbytes_kernel) * SECTOR_SIZE); */
        if (taskidx >= 0) 
            taskinfo[taskidx].app_sz = phyaddr - taskinfo[taskidx].app_img_offs;
        
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }

        fclose(fp);
        files++;
    }
    write_img_info(nbytes_kernel, phyaddr, taskinfo, tasknum, img);

    fclose(img);
}

static void create_image_vm(int nfiles, char *files[])
{
    int tasknum = nfiles - 3;
    int nbytes_kernel = 0;
    int nbytes_decompress = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;
    char *pbuff = buff;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 3;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);
        printf("%s starts from 0x%04lx\n", *files, phyaddr);
        
        /*copy app name  and img offset into taskinfo*/
        if (taskidx >= 0){
            memcpy(taskinfo[taskidx].app_name, *files, sizeof(*files));
            taskinfo[taskidx].app_name[sizeof(*files)] = 0;
            taskinfo[taskidx].app_img_offs = phyaddr;
        }

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            if (phdr.p_type != PT_LOAD) continue;

            /* write segment to the image */
            if (strcmp(*files, "main") != 0){
                if (strcmp(*files, "decompress") == 0)
                    nbytes_decompress += get_filesz(phdr);
                write_segment(phdr, fp, img, &phyaddr);
            }
            else{
                fseek(fp, phdr.p_offset, SEEK_SET);
                if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD){
                    fread(pbuff, sizeof(char), phdr.p_filesz, fp);
                    pbuff += phdr.p_filesz;
                }
                nbytes_kernel += get_filesz(phdr);
            }
        }

        if (taskidx >= 0) 
            taskinfo[taskidx].app_sz = phyaddr - taskinfo[taskidx].app_img_offs;
        
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }
        
        if (strcmp(*files, "decompress") == 0)
            write_padding(img, &phyaddr, SECTOR_SIZE + NBYTES2SEC(nbytes_decompress) * SECTOR_SIZE);
        
        if (strcmp(*files, "main") == 0){
            deflate_set_memory_allocator((void * (*)(int))malloc, free);
            struct libdeflate_compressor * compressor = deflate_alloc_compressor(1);

            // do compress
            int out_nbytes_kernel = deflate_deflate_compress(compressor, buff, nbytes_kernel, compress_buff, MAXSIZE);
            printf("original kernel size: 0x%04lx\n", nbytes_kernel);
            printf("compressed kernel size: 0x%04lx\n", out_nbytes_kernel);
            write_compressed_kernel(compress_buff, out_nbytes_kernel, img, &phyaddr);
        }

        fclose(fp);
        files++;
    }
    write_img_info_vm(nbytes_kernel, nbytes_decompress, phyaddr, taskinfo, tasknum, img);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1 || options.vm == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1 || options.vm == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_compressed_kernel(char* compress_buff, int out_nbytes_kernel, FILE *img, int *phyaddr)
{
    printf("\t\twriting 0x%04lx bytes for compressed kernel\n", out_nbytes_kernel);
    fwrite(compress_buff, sizeof(char), out_nbytes_kernel, img);
    *phyaddr += out_nbytes_kernel;
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (*phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(int nbytes_kernel, int app_info_offs, task_info_t *taskinfo, short tasknum, FILE * img)
{
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...
    printf("app_info starts from 0x%04lx\n", app_info_offs);
    fseek(img, OS_SIZE_LOC-8, SEEK_SET);
    fwrite(&app_info_offs, sizeof(int), 1, img);
    fwrite(&tasknum, sizeof(short), 1, img);
    fputc((char)(sizeof(task_info_t) % 256), img);
    fputc((char)(sizeof(task_info_t) / 256), img);
    fputc((char)(NBYTES2SEC(nbytes_kernel) % 256), img);
    fputc((char)(NBYTES2SEC(nbytes_kernel) / 256), img);
    fseek(img, app_info_offs, SEEK_SET);
    fwrite(taskinfo, sizeof(task_info_t), tasknum, img);
    for (int i = 0; i < tasknum; i++){
        printf("\tapp_name %s\n", taskinfo[i].app_name);
        printf("\t\timg_offset 0x%04lx", taskinfo[i].app_img_offs);
        printf("\t\tsize 0x%04lx\n", taskinfo[i].app_sz);
    }
}

static void write_img_info_vm(int nbytes_kernel, int nbytes_decompress, int app_info_offs, task_info_t *taskinfo, short tasknum, FILE * img)
{
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...
    printf("app_info starts from 0x%04lx\n", app_info_offs);
    fseek(img, OS_SIZE_LOC-12, SEEK_SET);
    fwrite(&nbytes_kernel, sizeof(int), 1, img);
    fwrite(&app_info_offs, sizeof(int), 1, img);
    fwrite(&tasknum, sizeof(short), 1, img);
    fputc((char)(sizeof(task_info_t) % 256), img);
    fputc((char)(sizeof(task_info_t) / 256), img);
    fputc((char)(NBYTES2SEC(nbytes_decompress) % 256), img);
    fputc((char)(NBYTES2SEC(nbytes_decompress) / 256), img);
    fseek(img, app_info_offs, SEEK_SET);
    fwrite(taskinfo, sizeof(task_info_t), tasknum, img);
    for (int i = 0; i < tasknum; i++){
        printf("\tapp_name %s\n", taskinfo[i].app_name);
        printf("\t\timg_offset 0x%04lx", taskinfo[i].app_img_offs);
        printf("\t\tsize 0x%04lx\n", taskinfo[i].app_sz);
    }
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}