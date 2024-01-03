#include <os/fs.h>
#include <os/sched.h>
char cat_buff[4096];
superblock_t sb;

void init_fs()
{
    uint64_t p = kva2pa(&sb);
    bios_sd_read(kva2pa(&sb), 1, B2SEC(0));
    if (sb.magic != 42){
        do_mkfs();
    }
    else{
        printk("[FS] start reading from superblock...\n");
        printk("[FS] magic : %d\n", sb.magic);
        printk("[FS] FS size : %dMiB, start sector : %lx\n", sb.fs_siz, 2<<20);
        printk("[FS] block size : 4KiB\n");
        printk("[FS] block map block offset : %d, block map block size : %d\n", sb.bmap_id, sb.bmap_num);
        printk("[FS] inode map block offset : %d, inode map block size : %d\n", sb.inmap_id, sb.inmap_num);
        printk("[FS] inode block offset : %d, inode block size : %d\n", sb.in_id, sb.in_num);
        printk("[FS] data block block offset : 14\n");
        printk("[FS] reading superblock finished!\n");
    }
}

void do_mkfs()
{
    int bcache_id;
    int ino;
    
    screen_move_cursor(0, 0);
    printk("[FS] start initializing filesystem          \n");
    printk("Setting superblock...\n");
    // Init superblock
    sb.magic     = 42;
    sb.sb_id     = 0;
    sb.sb_num    = 1;
    sb.bmap_id   = 1;
    sb.bmap_num  = 4;
    sb.inmap_id  = 5;
    sb.inmap_num = 1;
    sb.in_id     = 6;
    sb.in_num    = 8;
    sb.root_ino  = 0;
    sb.fs_siz    = 512;
    sb.bl_used   = 0;
    sb.in_used   = 0;
    bios_sd_write(kva2pa(&sb), 1, B2SEC(0));
    printk("[FS] magic : %d\n", sb.magic);
    printk("[FS] FS size : %dMiB, start sector : %lx\n", sb.fs_siz, 2<<20);
    printk("[FS] block size : 4KiB\n");
    printk("[FS] block map block offset : %d, block map block size : %d\n", sb.bmap_id, sb.bmap_num);
    printk("[FS] inode map block offset : %d, inode map block size : %d\n", sb.inmap_id, sb.inmap_num);
    printk("[FS] inode block offset : %d, inode block size : %d\n", sb.in_id, sb.in_num);
    printk("[FS] data block block offset : 14\n");
    
    // Inv bcache
    for (int i = 0; i < BCACHE_NR; i++){
        bcache[i].valid = 0;
    }
    
    // Init block_bitmap blocks
    printk("Setting block map...\n");
    for (int i = sb.bmap_id; i < sb.bmap_id + sb.bmap_num; i++){
        block_zero(i);
    } 
    bcache_id = bread(sb.bmap_id);
    (bcache[bcache_id].buff)[0] = 0xff;
    (bcache[bcache_id].buff)[1] = 0x3f;
    bwrite(bcache_id);
    for (int i = sb.bmap_id+1; i < sb.bmap_id + sb.bmap_num; i++){
        bcache_id = bread(i);
        bcache[bcache_id].valid = 0;
    }
    
    // Init inode_bitmap block
    printk("Setting inode map...\n");
    block_zero(sb.inmap_id);
    
    // Init inode and data blocks for root dir
    printk("Setting root dir\n");
    ino = ialloc();
    iinit(ino, T_DIR);
    dinit(ino, ino, "");
    
    printk("Initialize filesystem finished!\n");
    
    // Init proc's current working dir
    for (int i = 0; i < NUM_MAX_TASK; i++){
        if (pcb[i].status != TASK_EXITED){
            pcb[i].cwd = ROOT_INO;
            pcb[i].cwd_path[0] = '/';
            pcb[i].cwd_path[1] = '\0';
        }
    }
}

void do_statfs(char *buffer)
{
    int bcache_id;
    
    buffer[0] = '\0';
    strcat(buffer, "magic : 42 ");
    
    strcat(buffer, "block size : 4KiB\n");
    strcat(buffer, "total block num : 131072, ");
    strcat(buffer, "used  block num : ");
    char used[10];
    itoa(sb.bl_used, used, 10, 10);
    strcat(buffer, used);
    strcat(buffer, "\n");
    strcat(buffer, "total inode num : 1024, ");
    strcat(buffer, "used  inode num : ");
    itoa(sb.in_used, used, 10, 10);
    strcat(buffer, used);
    strcat(buffer, "\n");
    strcat(buffer, "inode  size : 32B, dentry size : 32B\n");
}

int do_enter_fs(char *path)
{
    int ino;
    int bcache_id;
    char name[NAMESIZ];
    inode_t inode;
    
    char *cwd_path = current_running[get_current_cpu_id()]->cwd_path;
    char *cwd_path_end = cwd_path + strlen(cwd_path);
    
    if ((ino = path_resolver(path, 0, name)) == -1){
        return -1;
    } 
    bcache_id = bread(INO2BLOCK(ino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
    if (inode.mode != T_DIR){
        return -1;
    }
    
    current_running[get_current_cpu_id()]->cwd = ino;
    if (*path == '/'){
        strcpy(cwd_path, path);
    }
    else if (*path == '.'){
        if (*(path+1) == '.'){
            if (*(path+2) != '/' && *(path+2) != '\0'){
                return -1;
            }
            while (*cwd_path_end != '/')
                cwd_path_end--;
            if (cwd_path_end == cwd_path && *(path+2) != '/'){
                *(cwd_path_end+1) = '\0';
            }
            else{
                *cwd_path_end = '\0';
            }
            while (*path != '/' && *path != '\0')
                path++;
            strcat(cwd_path, path);
        }
        else{
            while (*path != '/' && *path != '\0')
                path++;
            if (*(cwd_path_end-1) == '/')
                path++;
            strcat(cwd_path, path);
        }
    }
    else{
      if (strlen(current_running[get_current_cpu_id()]->cwd_path) > 1){
          strcat(cwd_path, "/");
      }
      strcat(cwd_path++, path);
    }
    return 1;
}

void do_get_cwdpath(char *buffer)
{
    strcpy(buffer, current_running[get_current_cpu_id()]->cwd_path);
}

int do_mkdir(char *path)
{
    int parino;
    char name[NAMESIZ];
    
    if ((parino = path_resolver(path, 1, name)) == -1){
        return -1;
    }
    int ino = ialloc();
    iinit(ino, T_DIR);
    dinit(ino, parino, name);
    return 1;
}

void rmfile(int ino, inode_t *ip)
{
    int block_id;
    int bcache_id;
    
    if (ip->mode == T_FIL && ip->link > 1){
        ip->link--;
        bcache_id = bread(INO2BLOCK(ino));
        memcpy(&((inode_t*)bcache[bcache_id].buff)[ino%128], ip, sizeof(inode_t));
        bwrite(bcache_id);
        return;
    }
    ifree(ino);
    for (int pos = 0; pos < ip->fsiz; pos += BLOCKSIZ){
        block_id = block_map(ip, pos / BLOCKSIZ);    
        bfree(block_id);
    }
    
}

void rmdir(int ino)
{
    int bcache_id;
    inode_t inode;
    dentry_t de;
    
    bcache_id = bread(INO2BLOCK(ino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
    if (inode.mode == T_FIL){
        rmfile(ino, &inode);
        return;
    }
    else{
        for (int pos = 2*sizeof(dentry_t); pos < inode.fsiz; pos += sizeof(dentry_t)){
            read_file(&inode, (char*)&de, pos, sizeof(dentry_t));
            rmdir(de.ino);
        }
        rmfile(ino, &inode);
    }
    return;
}

int do_rmdir(char *path)
{
    int parino;
    int ino;
    int bcache_id;
    inode_t inode;
    char name[NAMESIZ];
    
    if ((parino = path_resolver(path, 1, name)) == -1){
        return -1;
    }
    bcache_id = bread(INO2BLOCK(parino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[parino%128], sizeof(inode_t));
    if (inode.mode == T_FIL){
        return -1;
    }
    
    dentry_t de;
    int pos;
    for (pos = 0; pos < inode.fsiz; pos += sizeof(dentry_t)){
        read_file(&inode, (char*)&de, pos, sizeof(dentry_t));
        if (strcmp(name, de.name) == 0){
            ino = de.ino;
            goto find;
        }
    }
    return -1;
find:
    read_file(&inode, (char*)&de, inode.fsiz-sizeof(dentry_t), sizeof(dentry_t));
    write_file(&inode, (char*)&de, pos, sizeof(dentry_t));
    inode.fsiz -= sizeof(dentry_t);
    bcache_id = bread(INO2BLOCK(parino));
    memcpy(&((inode_t*)bcache[bcache_id].buff)[parino%128], &inode, sizeof(inode_t));
    bwrite(bcache_id);
    rmdir(ino);
    return 1;
}

int do_ls(char *path, char *buffer, int ls_l)
{
    int ino;
    int bcache_id;
    inode_t inode;
    dentry_t de;
    char name[NAMESIZ];
    
    if ((ino = path_resolver(path, 0, name)) == -1){
        return -1;
    }
    bcache_id = bread(INO2BLOCK(ino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
    if (inode.mode != T_DIR){
        return -1;
    }
    else{
        buffer[0] = '\0';
        if (ls_l == 1)
            goto more_info;
        for (int pos = 0; pos < inode.fsiz; pos += sizeof(dentry_t)){
            read_file(&inode, (char*)&de, pos, sizeof(dentry_t));
            strcat(buffer, de.name);
            strcat(buffer, " ");
        }
        return 1;
        more_info:
            for (int pos = 0; pos < inode.fsiz; pos += sizeof(dentry_t)){
                read_file(&inode, (char*)&de, pos, sizeof(dentry_t));
                strcat(buffer, "name: ");
                strcat(buffer, de.name);
                strcat(buffer, " ino: ");
                inode_t ls_inode;
                int ls_bcache_id;
                ls_bcache_id = bread(INO2BLOCK(de.ino));
                memcpy(&ls_inode, &((inode_t*)bcache[ls_bcache_id].buff)[de.ino%128], sizeof(inode_t));
                char tmp_buff[16];
                itoa(de.ino, tmp_buff, 10, 10);
                strcat(buffer, tmp_buff);
                strcat(buffer, " link_num: ");
                itoa(ls_inode.link, tmp_buff, 10, 10);
                strcat(buffer, tmp_buff);
                strcat(buffer, " file_siz: ");
                itoa(ls_inode.fsiz, tmp_buff, 10, 10);
                strcat(buffer, tmp_buff);
                strcat(buffer, "\n");
            }
            
    }
    return 1;
}

int do_create(char *path)
{
    int parino;
    int bcache_id;
    inode_t inode;
    char name[NAMESIZ];
    
    if ((parino = path_resolver(path, 1, name)) == -1){
        return -1;
    }
    bcache_id = bread(INO2BLOCK(parino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[parino%128], sizeof(inode_t));
    if (inode.mode != T_DIR){
        return -1;
    }
    int ino = ialloc();
    iinit(ino, T_FIL);
    finit(ino, parino, name);
    return ino;
}

int do_open(char *path, int access)
{
    char name[NAMESIZ];
    int ino;
    int bcache_id;
    inode_t inode;
    int empty = -1;
    pcb_t *proc = current_running[get_current_cpu_id()];
    
    if ((ino = path_resolver(path, 0, name)) == -1){
        if ((ino = do_create(path)) == -1){
            return -1;
        }
    }
    
    bcache_id = bread(INO2BLOCK(ino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
    if (inode.mode != T_FIL){
        return -1;
    }
    
    for (int i = 0; i < MAX_OPEN_FILE; i++){
        if (proc->open_file[i].valid == 1 && proc->open_file[i].ino == ino){
            proc->open_file[i].access = access;
            return i;
        }
        else if(current_running[get_current_cpu_id()]->open_file[i].valid == 0 && empty == -1){
            empty = i;
        }
    }
    if (empty == -1){
        return -1;
    }
    else{
        proc->open_file[empty].valid = 1;
        proc->open_file[empty].ino = ino;
        proc->open_file[empty].access = access;
        proc->open_file[empty].pos = 0;
        return empty;
    }
}

int do_read(int fd, char *buffer, int sz)
{
    inode_t inode;
    int bcache_id;
    pcb_t *proc = current_running[get_current_cpu_id()];
    
    if (fd < 0 || fd >= MAX_OPEN_FILE){
        return -1;
    }
    
    if (proc->open_file[fd].valid == 0 || proc->open_file[fd].access == O_WRONLY){
        return -1;
    }
    int ino = proc->open_file[fd].ino;
    bcache_id = bread(INO2BLOCK(ino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
    int real_sz = read_file(&inode, buffer, proc->open_file[fd].pos, sz);
    proc->open_file[fd].pos += real_sz;
    return real_sz;
}

int do_write(int fd, char *buffer, int sz)
{
    inode_t inode;
    int bcache_id;
    pcb_t *proc = current_running[get_current_cpu_id()];
    
    if (fd < 0 || fd >= MAX_OPEN_FILE){
        return -1;
    }
    
    if (proc->open_file[fd].valid == 0 || proc->open_file[fd].access == O_RDONLY){
        return -1;
    }
    int ino = proc->open_file[fd].ino;
    bcache_id = bread(INO2BLOCK(ino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
    int real_sz = write_file(&inode, buffer, proc->open_file[fd].pos, sz);
    if (proc->open_file[fd].pos + real_sz > inode.fsiz){
        inode.fsiz = proc->open_file[fd].pos + real_sz;
    }
    proc->open_file[fd].pos += real_sz;
    bcache_id = bread(INO2BLOCK(ino));
    memcpy(&((inode_t*)bcache[bcache_id].buff)[ino%128], &inode, sizeof(inode_t));
    bwrite(bcache_id);
    return real_sz;
}

void do_close(int fd)
{
    pcb_t *proc = current_running[get_current_cpu_id()];
    if (fd >= 0 && fd < MAX_OPEN_FILE){
        proc->open_file[fd].valid = 0;
    }
}

int do_cat(char *path)
{
    char name[NAMESIZ];
    int bcache_id;
    int ino;
    inode_t inode;
    
    if ((ino = path_resolver(path, 0, name)) == -1){
        return -1;
    }
    bcache_id = bread(INO2BLOCK(ino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
    if (inode.mode != T_FIL){
        return -1;
    }
    read_file(&inode, cat_buff, 0, inode.fsiz);
    cat_buff[inode.fsiz] = '\0';
    screen_move_cursor(0, 0);
    printk("%s\n", cat_buff);
}

void falloc(inode_t *ip, int begin, int end)
{
    int begin_block = begin / BLOCKSIZ + 1;
    int end_block   = end / BLOCKSIZ;
    for (int i = begin_block; i <= end_block; i++){
        block_map(ip, i);
    }
}

int do_lseek(int fd, int offset, int whence)
{
    pcb_t *proc = current_running[get_current_cpu_id()];
    
    if (fd < 0 || fd >= MAX_OPEN_FILE){
        return -1;
    }
    if (proc->open_file[fd].valid == 0){
        return -1;
    }
    if (whence == SEEK_SET){
        proc->open_file[fd].pos = offset;
    }
    else if (whence == SEEK_CUR){
        proc->open_file[fd].pos += offset;
    }
    else if (whence == SEEK_END){
        int bcache_id = bread(INO2BLOCK(proc->open_file[fd].ino));
        inode_t inode;
        memcpy(&((inode_t*)bcache[bcache_id].buff)[proc->open_file[fd].ino%128], &inode, sizeof(inode_t));
        proc->open_file[fd].pos = inode.fsiz + offset;
    }
    else{
        return -1;
    }
    
    inode_t inode;
    int ino = proc->open_file[fd].ino;
    int bcache_id = bread(INO2BLOCK(ino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
    if (proc->open_file[fd].pos > inode.fsiz){
        falloc(&inode, inode.fsiz, proc->open_file[fd].pos);
        inode.fsiz = proc->open_file[fd].pos;
        bcache_id = bread(INO2BLOCK(ino));
        memcpy(&((inode_t*)bcache[bcache_id].buff)[ino%128], &inode, sizeof(inode_t));
        bwrite(bcache_id);
    }
    return 1;
}

int do_ln(inode_t *ip, int fil_ino, char *name)
{
    dentry_t de;
    
    de.ino = fil_ino;
    strcpy(de.name, name);
    if (write_file(ip, &de, ip->fsiz, sizeof(dentry_t)) == sizeof(dentry_t)){
        return 1;
    }
    else{
        return -1;
    }
    
}

int do_link(char *fil_path, char *dir_path)
{
    char fil_name[NAMESIZ];
    char dir_name[NAMESIZ];
    int bcache_id;
    int ino, fil_ino, dir_ino;
    inode_t inode, dir_inode, fil_inode;
    
    if ((dir_ino = path_resolver(dir_path, 1, dir_name)) == -1){
        return -1;
    }
    if ((fil_ino = path_resolver(fil_path, 0, fil_name)) == -1){
        return -1;
    }
    bcache_id = bread(INO2BLOCK(dir_ino));
    memcpy(&dir_inode, &((inode_t*)bcache[bcache_id].buff)[dir_ino%128], sizeof(inode_t));
    ino = dirlookup(&dir_inode, dir_name);
    if (ino == -1 && dir_name[0] != '\0'){
        if (do_ln(&dir_inode, fil_ino, dir_name) == -1)
            return -1;
        dir_inode.fsiz += sizeof(dentry_t);
        bcache_id = bread(INO2BLOCK(dir_ino));
        memcpy(&((inode_t*)bcache[bcache_id].buff)[dir_ino%128], &dir_inode, sizeof(inode_t));
        bwrite(bcache_id);
    }
    else if (ino == -1 && dir_name[0] == '\0'){
        if (do_ln(&dir_inode, fil_ino, fil_name) == -1)
            return -1;
        dir_inode.fsiz += sizeof(dentry_t);
        bcache_id = bread(INO2BLOCK(dir_ino));
        memcpy(&((inode_t*)bcache[bcache_id].buff)[dir_ino%128], &dir_inode, sizeof(inode_t));
        bwrite(bcache_id);
    }
    else{
        bcache_id = bread(INO2BLOCK(ino));
        memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
        if (inode.mode == T_FIL){
            return -1;
        }
        else{
            if (do_ln(&inode, fil_ino, fil_name) == -1)
                return -1;
            inode.fsiz += sizeof(dentry_t);
            bcache_id = bread(INO2BLOCK(ino));
            memcpy(&((inode_t*)bcache[bcache_id].buff)[ino%128], &inode, sizeof(inode_t));
            bwrite(bcache_id);
        }
    }
    
    bcache_id = bread(INO2BLOCK(fil_ino));
    memcpy(&fil_inode, &((inode_t*)bcache[bcache_id].buff)[fil_ino%128], sizeof(inode_t));
    fil_inode.link++;
    memcpy(&((inode_t*)bcache[bcache_id].buff)[fil_ino%128], &fil_inode, sizeof(inode_t));
    bwrite(bcache_id);
    return 1;
}
