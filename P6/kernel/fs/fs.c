#include <os/fs.h>
#include <os/sched.h>

bcache_t bcache[BCACHE_NR];
uint64_t seed = 1;
char cwd_name[2] = {'.', '\0'};
char pwd_name[3] = {'.', '.', '\0'};

int read_file(inode_t *ip, char *buff, int pos, int sz)
{
    int m, block_id, bcache_id;
    
    if (pos + sz > ip->fsiz){
        sz = ip->fsiz - pos;
    }
    for (int total = 0; total < sz; total += m, pos += m, buff += m){
        block_id = block_map(ip, pos / BLOCKSIZ);
        bcache_id = bread(block_id);
        m = MIN(sz-total, BLOCKSIZ-pos % BLOCKSIZ);
        memcpy(buff+total, &(bcache[bcache_id].buff[pos % BLOCKSIZ]), m);
    }
    return sz;
}

int write_file(inode_t *ip, char *buff, int pos, int sz)
{
    int m, block_id, bcache_id;
    for (int total = 0; total < sz; total += m, pos += m, buff += m){
        block_id = block_map(ip, pos / BLOCKSIZ);
        bcache_id = bread(block_id);
        m = MIN(sz-total, BLOCKSIZ-pos % BLOCKSIZ);
        memcpy(&(bcache[bcache_id].buff[pos % BLOCKSIZ]), buff+total, m);
        bwrite(bcache_id);
    }
    return sz;
}

void dinit(int ino, int parino, char *name)
{
    dentry_t de;
    inode_t inode;
    int bcache_id;
    
    bcache_id = bread(INO2BLOCK(ino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
    
    de.ino = ino;
    strcpy(de.name, cwd_name);
    write_file(&inode, (char*)&de, 0, sizeof(dentry_t));
    
    de.ino = parino;
    strcpy(de.name, pwd_name);
    write_file(&inode, (char*)&de, sizeof(dentry_t), sizeof(dentry_t));
    
    inode.fsiz += 2 * sizeof(dentry_t);
    bcache_id = bread(INO2BLOCK(ino));
    memcpy(&((inode_t*)bcache[bcache_id].buff)[ino%128], &inode, sizeof(inode_t));
    bwrite(bcache_id);
    
    if (ino != parino){
        bcache_id = bread(INO2BLOCK(parino));
        memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[parino%128], sizeof(inode_t));
        de.ino = ino;
        strcpy(de.name, name);
        write_file(&inode, (char*)&de, inode.fsiz, sizeof(dentry_t));
        inode.fsiz += sizeof(dentry_t);
        bcache_id = bread(INO2BLOCK(parino));
        memcpy(&((inode_t*)bcache[bcache_id].buff)[parino%128], &inode, sizeof(inode_t));
        bwrite(bcache_id);
    }
}

void finit(int ino, int parino, char *name)
{
    dentry_t de;
    inode_t inode;
    int bcache_id;
    
    bcache_id = bread(INO2BLOCK(parino));
    memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[parino%128], sizeof(inode_t));
    de.ino = ino;
    strcpy(&de.name, name);
    write_file(&inode, (char*)&de, inode.fsiz, sizeof(dentry_t));
    inode.fsiz += sizeof(dentry_t);
    bcache_id = bread(INO2BLOCK(parino));
    memcpy(&((inode_t*)bcache[bcache_id].buff)[parino%128], &inode, sizeof(inode_t));
    bwrite(bcache_id);
}

int ialloc()
{
    int bcache_id;
    for (int i = 0; i < INO_NUM; i++){
        bcache_id = bread(sb.inmap_id);
        if ((bcache[bcache_id].buff)[i] == 0){
            (bcache[bcache_id].buff)[i] = 1;
            bwrite(bcache_id);
            sb.in_used++;
            bios_sd_write(kva2pa(&sb), 1, B2SEC(0));
            return i;
        }
    }
    screen_move_cursor(0, 0);
    printk("panic: no more inode to alloc\n");
    while (1)
    {
        asm volatile("wfi");
    }
}

void iinit(int ino, char mode)
{
    int bcache_id;
    inode_t *ip;
    
    bcache_id = bread(INO2BLOCK(ino));
    ip = &((inode_t*)bcache[bcache_id].buff)[ino%128];
    ip->mode  = mode;
    ip->owner = current_running[get_current_cpu_id()]->pid;
    ip->link  = 1;
    ip->fsiz  = 0;
    ip->time  = get_timer();
    for (int i = 0; i < NRDIR+2; i++){
        ip->addr[i] = 0;
    }
    bwrite(bcache_id);
}

void ifree(int ino)
{
    int bcache_id;
    bcache_id = bread(sb.inmap_id);
    (bcache[bcache_id].buff)[ino] = 0;
    bwrite(bcache_id);
    sb.in_used--;
    bios_sd_write(kva2pa(&sb), 1, B2SEC(0));
}

void block_zero(int block_id)
{
    int bcache_id;
    int empty = -1;
    
    for (int i = 0; i < BCACHE_NR; i++){
        if (bcache[i].valid && bcache[i].block_id == block_id){
            bcache_id = i;
            goto find;
        }
        if (bcache[i].valid == 0 && empty == -1){
            empty = i;
        }
    }
    if (empty != -1){
        bcache[empty].valid = 1;
        bcache_id = empty;
    }
    else {
        bcache_id = rand();
    }
find:
    bcache[bcache_id].block_id = block_id;
    memset(bcache[bcache_id].buff, 0, BLOCKSIZ);
    bwrite(bcache_id);
}

int balloc(int block_type)
{
    int bcache_id, b;
    
    for (b = sb.bmap_id; b < sb.bmap_id + sb.bmap_num; b++){
        bcache_id = bread(b);
        for (int bi = 0; bi < (1<<15); bi++){
            int m = 1 << (bi % 8);
            if ((bcache[bcache_id].buff[bi/8] & m) == 0){
                bcache[bcache_id].buff[bi/8] |= m;
                bwrite(bcache_id);
                if (block_type == INDIRBLOCK){
                    block_zero((b-1)*(1<<15)+bi);
                }
                sb.bl_used++;
                bios_sd_write(kva2pa(&sb), 1, B2SEC(0));
                return ((b-1)*(1<<15)+bi);
            }
        }
    }
    screen_move_cursor(0, 0);
    printk("panic: no more block to alloc\n");
    while (1)
    {
        asm volatile("wfi");
    }
}

void bfree(int block_id)
{
    int b = block_id / (1<<15);
    int bcache_id;
    bcache_id = bread(b);
    int m = 1 << (block_id % 8);
    bcache[bcache_id].buff[block_id/8] &= ~m;
    bwrite(bcache_id);
    sb.bl_used--;
    bios_sd_write(kva2pa(&sb), 1, B2SEC(0));
}

int rand()
{
    seed = seed * 1103515245 + 12345;
    return (uint64_t)(seed / 65536) % BCACHE_NR;
}

int bread(int block_id)
{
    int empty = -1;
    for (int i = 0; i < BCACHE_NR; i++){
        if (bcache[i].valid && bcache[i].block_id == block_id){
            return i;
        }
        if (bcache[i].valid == 0 && empty == -1){
            empty = i;
        }
    }
    if (empty != -1){
        bios_sd_read(kva2pa(bcache[empty].buff), 8, B2SEC(block_id));
        bcache[empty].valid = 1;
        bcache[empty].block_id = block_id;
        return empty;
    }
    else {
        int rand_num = rand();
        bios_sd_read(kva2pa(bcache[rand_num].buff), 8, B2SEC(block_id));
        bcache[rand_num].block_id = block_id;
        return rand_num;
    }
}

void bwrite(int bcache_id)
{
    bios_sd_write(kva2pa(bcache[bcache_id].buff), 8, B2SEC(bcache[bcache_id].block_id));
}

int block_map(inode_t *ip, int block_id)
{
    int bcache_id;
    int bcachelo_id, bcachehi_id;
    int lo, hi;
    
    if (block_id < NRDIR){
        if (ip->addr[block_id] == 0){
            ip->addr[block_id] = balloc(DATABLOCK);
        }
        return ip->addr[block_id];
    }
    else if (block_id < NRDIR + NRINDIR1){
        if (ip->addr[NRDIR] == 0){
            ip->addr[NRDIR] = balloc(INDIRBLOCK); 
        }
        bcache_id = bread(ip->addr[NRDIR]);
        if (((int*)bcache[bcache_id].buff)[block_id-NRDIR] == 0){
            ((int*)bcache[bcache_id].buff)[block_id-NRDIR] = balloc(DATABLOCK);
            bwrite(bcache_id);
        }
        return ((int*)bcache[bcache_id].buff)[block_id-NRDIR];
    }
    else if (block_id < NRDIR + NRINDIR1 + NRINDIR2){
        if (ip->addr[NRDIR+1] == 0){
            ip->addr[NRDIR+1] = balloc(INDIRBLOCK); 
        }
        bcachehi_id = bread(ip->addr[NRDIR+1]);
        hi = (block_id - NRDIR - NRINDIR1) >> 10;
        lo = (block_id - NRDIR - NRINDIR1) & 1023;
        if (((int*)bcache[bcachehi_id].buff)[hi] == 0){
            ((int*)bcache[bcachehi_id].buff)[hi] = balloc(INDIRBLOCK);
            bwrite(bcachehi_id);
        }
        bcachelo_id = bread((int*)(bcache[bcachehi_id].buff)[hi]);
        if (((int*)bcache[bcachelo_id].buff)[lo] == 0){
            ((int*)bcache[bcachelo_id].buff)[lo] = balloc(DATABLOCK);
            bwrite(bcachelo_id);
        }
        return ((int*)bcache[bcachelo_id].buff)[lo];
    }
}

int dirlookup(inode_t *ip, char *name)
{
    dentry_t de;
    for (int pos = 0; pos < ip->fsiz; pos += sizeof(dentry_t)){
        read_file(ip, (char*)&de, pos, sizeof(dentry_t));
        if (strcmp(name, de.name) == 0){
            return de.ino;
        }
    }
    return -1;
}
char* name_resolver(char *path, char *name)
{
    int pos = 0;
    
    while (*path == '/')
        path++;
    if (*path == '\0'){
        return 0;
    }
    while (*path != '/' && *path != '\0'){
        if (pos < NAMESIZ-1){
            name[pos++] = *path;
        }    
        path++;
    }
    name[pos] = '\0';
    while (*path == '/')
        path++;
    return path;
}

int path_resolver(char *path, int pardir, char *name)
{
    int ino;
    int bcache_id;
    inode_t inode;
    
    if (path == 0){
        return current_running[get_current_cpu_id()]->cwd;
    }
    if (*path == '/'){
        ino = ROOT_INO;
    }
    else if (*path == '.'){
        if (*(path+1) == '.'){
            dentry_t de;
            int cwd_ino = current_running[get_current_cpu_id()]->cwd;
            bcache_id = bread(INO2BLOCK(cwd_ino));
            memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[cwd_ino%128], sizeof(inode_t));
            read_file(&inode, &de, sizeof(dentry_t), sizeof(dentry_t));
            ino = de.ino;
            path += 2;
            if (*path == '.'){
                return -1;
            }
        }
        else{
            ino = current_running[get_current_cpu_id()]->cwd;
            path++;
        }
    }
    else{
        ino = current_running[get_current_cpu_id()]->cwd;
    }
    
    while ((path = name_resolver(path, name)) != 0){
        bcache_id = bread(INO2BLOCK(ino));
        memcpy(&inode, &((inode_t*)bcache[bcache_id].buff)[ino%128], sizeof(inode_t));
        if (inode.mode != T_DIR){
            return -1;
        }
        
        if (pardir == 1 && *path == '\0'){
            return ino;
        }
        
        if ((ino = dirlookup(&inode, name)) == -1){
            return -1;
        }
    }
    return ino;
}