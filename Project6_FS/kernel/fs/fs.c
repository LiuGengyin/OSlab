#include <os/string.h>
#include <pgtable.h>
#include <os/fs.h>
#include <os/sched.h>
#include <os/kernel.h>
#include <assert.h>

static superblock_t superblock;
static fdesc_t fdesc_array[NUM_FDESCS];
dentry_t dentrys[NUM_DENTRYS];
char blockmap[32*8*512];

/*
位图：以字节粒度在内存中读写，无关大小端序，字节的高位代表位图的低位。比如，填充位图首个元素后如下：
字节 0     1    ...
内容 0x80  0x00 ...
*/

int do_mkfs(void)
{
    // TODO [P6-task1]: Implement do_mkfs
    //写超级块
    superblock.magic = SUPERBLOCK_MAGIC;
    superblock.fssz = 131085;
    superblock.phy_start_sec = FS_START_SEC;
    
    superblock.blockmap_blk = 1;
    superblock.blockmap_sz = 32;
    
    superblock.inodemap_blk = 33;
    superblock.inodemap_sz = 1;
    
    superblock.inode_blk = 34;
    superblock.inode_sz = 8; //512 inodes, 64B per inode
    
    superblock.data_blk = 42;
    superblock.data_sz = 0x20000; //2E17 blocks = 512MB
    bios_sdwrite(kva2pa(&superblock), 1, FS_START_SEC);
    
    //初始化数据位图blockmap
    char buff[512];
    for(int i = 0; i < 512; i++)
        buff[i] = 0;
        
    for(int i = BLKMAP_START_SEC; i < INODEMAP_START_SEC; i++)
        bios_sdwrite(kva2pa(buff), 1, i);
        
    //初始化inode位图inodemap
    for(int i = INODEMAP_START_SEC; i < INODE_START_SEC; i++)
        bios_sdwrite(kva2pa(buff), 1, i);
        
    //创建根目录（根目录总占据首个inode）
    buff[0] = 0x80;
    bios_sdwrite(kva2pa(buff), 1, INODEMAP_START_SEC);
    buff[0] = 0xf0; //512个目录项占4块
    bios_sdwrite(kva2pa(buff), 1, BLKMAP_START_SEC);
    inode_t inodebuf;
    inodebuf.inodeid = 0;
    inodebuf.refer = -1;
    inodebuf.mode = MODE_NULL;
    inodebuf.used = 2; //., ..
    for(int i = 0; i < 4; i++) inodebuf.ptr0[i] = i;
    for(int i = 4; i < 7; i++) inodebuf.ptr0[i] = -1;
    for(int i = 0; i < 3; i++) inodebuf.ptr1[i] = -1;
    for(int i = 0; i < 2; i++) inodebuf.ptr2[i] = -1;
    bios_sdwrite(kva2pa(&inodebuf), 1, INODE_START_SEC);
    
    //考虑到会在用户程序中mkfs，而用户的内核栈只有4KB，需要做些处理
    dentry_t dentrybuf[NUM_DENTRYS / 32]; //限制数组大小为512字节
    dentrybuf[0].inodeid = 0;
    dentrybuf[0].type = TYPE_ANCES;
    strcpy(dentrybuf[0].name, "root");
    dentrybuf[1].inodeid = 0;
    dentrybuf[1].type = TYPE_SELF;
    strcpy(dentrybuf[1].name, "root");
    for(int i = 2; i < NUM_DENTRYS / 32; i++){ //根目录其余初始为空
        dentrybuf[i].inodeid = -1;
        dentrybuf[i].type = TYPE_NULL;
        dentrybuf[i].name[0] = '\0';
    }
    bios_sdwrite(kva2pa(dentrybuf), 1, DATA_START_SEC);
    
    for(int j = 0; j < NUM_DENTRYS / 32; j++){
            dentrybuf[j].inodeid = -1;
            dentrybuf[j].type = TYPE_NULL;
            dentrybuf[j].name[0] = '\0';
    }
    for(int i = 1; i < 32; i++){
        bios_sdwrite(kva2pa(dentrybuf), 1, DATA_START_SEC + i);
    }
    
    if(/*current_running->pid != 0*/1){
        screen_move_cursor(0, 0);
        printk("[FS] Start initializing filesystem!\n");
        printk("     inode size: 64B, dentry size: 32B\n");
        printk("[FS] Setting superblock...\n");
        printk("     magic: %d\n", superblock.magic);
        printk("     fs size: %d blocks\n", superblock.fssz);
        printk("     blockmap start from block %d, and occupies %d blocks\n", superblock.blockmap_blk, superblock.blockmap_sz);
        printk("     inodemap start from block %d, and occupies %d blocks\n", superblock.inodemap_blk, superblock.inodemap_sz);
        printk("     inodes start from block %d, and occupies %d blocks\n", superblock.inode_blk, superblock.inode_sz);
        printk("     data start from block %d, and occupies %d blocks\n", superblock.data_blk, superblock.data_sz);
        printk("[FS] Initialized filesystem successfully!\n");
    }

    return 0;  // do_mkfs succeeds
}

int do_statfs(void)
{
    // TODO [P6-task1]: Implement do_statfs
    int used_inodes = 0;
    int used_data_blocks = 0;
    char buff[512];
    bios_sdread(kva2pa(buff), 1, INODEMAP_START_SEC);
    for(int i = 0; i < 64; i++) //inodemap只有前512位有效
        while(buff[i] != 0) {buff[i] &= buff[i] - 1; used_inodes++;}
    
    for(int i = 0; i < 32 * 8; i++){
        bios_sdread(kva2pa(buff), 1, BLKMAP_START_SEC + i);
        for(int i = 0; i < 512; i++)
            while(buff[i] != 0) {buff[i] &= buff[i] - 1; used_data_blocks++;}
    }
    
    screen_move_cursor(0, 0);
    printk("[FS] inodes used %d/512\n", used_inodes);
    printk("[FS] data blocks used %d/131,072\n", used_data_blocks);
    

    return 0;  // do_statfs succeeds
}

int do_cd(char *path, int p)
{
    // TODO [P6-task1]: Implement do_cd
    if(path[0] == '\0') {current_running->cur_dir_inodeid = 0; return 0;}
    
    int cur_dir_inodeid = current_running->cur_dir_inodeid;
    inode_t inodes[NUM_INODES / 64];
    inode_t cur_dir_inode;
    int find = 0;
    for(int i = 0; i < 64; i++){
        bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + i);
        for(int j = 0; j < NUM_INODES / 64; j++){
            if(cur_dir_inodeid == inodes[j].inodeid){
                cur_dir_inode = inodes[j];
                find = 1;
                goto here;
            }
        }
    }
here:
    /*bios_sdread(kva2pa(inodes), 64, INODE_START_SEC);
    inode_t cur_dir_inode;
    int j;
    for(j = 0; j < NUM_INODES; j++){
        if(cur_dir_inodeid == inodes[j].inodeid){
            cur_dir_inode = inodes[j];
            break;
        }
    }
    if(j == NUM_INODES) {printk("ERROR: CAN'T FIND CURRENT DIR\n"); return -1;}*/
    if(!find) {return -1;}
    
    //由于目录只有512个目录项，共占有16KB，只会用到直接索引。接下来读出四个数据块
    //16K太大了所以放在数据段里
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
    }
    
    //parse
    if(strncmp(path, "..", 2) == 0){
        for(int i = 0; i < NUM_DENTRYS; i++)
            if(dentrys[i].type == TYPE_ANCES){
                current_running->cur_dir_inodeid = dentrys[i].inodeid;
                return 0;
            }
    }    
    else if(path[0] == '.' && path[1] != '.'){
        return 0;
    }
    else{
        char nextdir[30];
        int i;
        for(i = 0; path[i] != '/' && path[i] != '\0'; i++){
            nextdir[i] = path[i];
        }
        nextdir[i] = '\0';
        if(path[i] == '\0') path -= 1;
        path += i+1;
        for(i = 0; i < NUM_DENTRYS; i++){
            if(strcmp(nextdir, dentrys[i].name) == 0 && dentrys[i].type != TYPE_FILE){
                current_running->cur_dir_inodeid = dentrys[i].inodeid;
                break;
            }
        }
        if(i == NUM_DENTRYS) {return -2;}
        else if(path[0] == '\0'){
            screen_move_cursor(0, 0);
            int m;
            for(m = 0; m < NUM_DENTRYS; m++)
                if(dentrys[m].type == TYPE_SELF) break;
            if(p) printk("now you are in dir: %s, previous dir is: %s\n", dentrys[i].name, dentrys[m].name);
            return 0;
        }
        else do_cd(path, 1);
    }

    return 0;  // do_cd succeeds
}

int do_mkdir(char *path)
{
    // TODO [P6-task1]: Implement do_mkdir
    int cur_dir_inodeid = current_running->cur_dir_inodeid;
    inode_t inodess[NUM_INODES / 64];
    inode_t cur_dir_inode;
    int find = 0;
    int ii, jj;
    for(ii = 0; ii < 64; ii++){
        bios_sdread(kva2pa(inodess), 1, INODE_START_SEC + ii);
        for(jj = 0; jj < NUM_INODES / 64; jj++){
            if(cur_dir_inodeid == inodess[jj].inodeid){
                cur_dir_inode = inodess[jj];
                find = 1;
                goto here1;
            }
        }
    }
here1:
    if(!find) {return -1;}
    
    //检查当前目录中是否有同名子目录
    char ances_name[30];
    int ances_id;
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
    }
    for(int i = 0; i < NUM_DENTRYS; i++)
        if(dentrys[i].type == TYPE_SELF){strcpy(ances_name, dentrys[i].name); ances_id = dentrys[i].inodeid; break;}
    int j;
    for(j = 0; j < NUM_DENTRYS; j++){
        if(strcmp(path, dentrys[j].name) == 0 && dentrys[j].type != TYPE_FILE) break;
    }
    if(j != NUM_DENTRYS) {return -2;}
    
    //增加当前目录的硬链接数和used数
    inodess[jj].refer++;
    inodess[jj].used++;
    bios_sdwrite(kva2pa(inodess), 1, INODE_START_SEC + ii);
    
    //分配目录项
    for(j = 0; j < NUM_DENTRYS; j++){
        if(dentrys[j].type == TYPE_NULL) break;
    }
    assert(j != NUM_DENTRYS);
    dentrys[j].type = TYPE_DIR;
    strcpy(dentrys[j].name, path);
    int create_inodeid;
    char inodemap[512];
    bios_sdread(kva2pa(inodemap), 1, INODEMAP_START_SEC);
    int k;
    //分配inode
    for(k = 0; k < 64; k++){
        if(inodemap[k] != 0xff) break;
    }
    assert(k != 64);
    int firstzero;
    char inodemapbuf = inodemap[k];
    for(int i = 0; i < 8; i++){
        if(!(inodemapbuf & 0x80)) {firstzero = i; break;}
        else{
            inodemapbuf = inodemapbuf << 1;
        }
    }
    create_inodeid = 8*k+firstzero; //注意位图的定义
    inodemap[k] |= (1 << (7-firstzero));
    bios_sdwrite(kva2pa(inodemap), 1, INODEMAP_START_SEC);
        
    dentrys[j].inodeid = create_inodeid; //父目录项初始化完成
    for(int i = 0; i < 4; i++){
        bios_sdwrite(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
    }
    
    //初始化待创建目录inode 
    inode_t inodes[8];
    inode_t inode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + create_inodeid / 8);  
    inode.inodeid = create_inodeid;
    inode.refer = 1;
    inode.mode = MODE_NULL;
    inode.used = 2;
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(blockmap + 64*512*i), 64, BLKMAP_START_SEC + 64 * i);
    }
    for(int i = 0; i < 4; i++){
        for(k = 0; k < 32*8*512; k++){
            if(blockmap[k] != 0xff) break;
        }
        assert(k != 32*8*512);
        char blkmapbuf = blockmap[k];
        for(int m = 0; m < 8; m++){
            if(!(blkmapbuf & 0x80)) {firstzero = m; break;}
            else blkmapbuf = blkmapbuf << 1;
        }
        inode.ptr0[i] = 8*k+firstzero;
        blockmap[k] |= (1 << (7-firstzero));
    }
    for(int i = 0; i < 4; i++)
        bios_sdwrite(kva2pa(blockmap + 64*512*i), 64, BLKMAP_START_SEC + 64 * i);
    for(int i = 4; i < 7; i++) inode.ptr0[i] = -1;
    for(int i = 0; i < 3; i++) inode.ptr1[i] = -1;
    for(int i = 0; i < 2; i++) inode.ptr2[i] = -1;  
    inodes[create_inodeid % 8] = inode;
    bios_sdwrite(kva2pa(inodes), 1, INODE_START_SEC + create_inodeid / 8);
    
    //初始化被创建目录的目录项
    dentry_t dentrybuf[NUM_DENTRYS / 32]; //限制数组大小为512字节
    dentrybuf[0].inodeid = ances_id;
    dentrybuf[0].type = TYPE_ANCES;
    strcpy(dentrybuf[0].name, ances_name);
    dentrybuf[1].inodeid = create_inodeid;
    dentrybuf[1].type = TYPE_SELF;
    strcpy(dentrybuf[1].name, path);
    for(int i = 2; i < NUM_DENTRYS / 32; i++){
        dentrybuf[i].inodeid = -1;
        dentrybuf[i].type = TYPE_NULL;
        dentrybuf[i].name[0] = '\0';
    }
    bios_sdwrite(kva2pa(dentrybuf), 1, DATA_START_SEC + inode.ptr0[0]*8); //首个数据块，由ptr0[0]指示
    for(int i = 0; i < NUM_DENTRYS / 32; i++){
        dentrybuf[i].inodeid = -1;
        dentrybuf[i].type = TYPE_NULL;
        dentrybuf[i].name[0] = '\0';
    }
    for(int i = 1; i < 8; i++){
        bios_sdwrite(kva2pa(dentrybuf), 1, DATA_START_SEC + inode.ptr0[0]*8 + i);
    }
    for(int m = 1; m < 4; m++)
        for(int i = 0; i < 8; i++)
            bios_sdwrite(kva2pa(dentrybuf), 1, DATA_START_SEC + inode.ptr0[m]*8 + i);
    
    return 0;  // do_mkdir succeeds
}

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir
    int cur_dir_inodeid = current_running->cur_dir_inodeid;
    inode_t inodess[NUM_INODES / 64];
    inode_t cur_dir_inode;
    int find = 0;
    int ii, jj;
    for(ii = 0; ii < 64; ii++){
        bios_sdread(kva2pa(inodess), 1, INODE_START_SEC + ii);
        for(jj = 0; jj < NUM_INODES / 64; jj++){
            if(cur_dir_inodeid == inodess[jj].inodeid){
                cur_dir_inode = inodess[jj];
                find = 1;
                goto here2;
            }
        }
    }
here2:
    if(!find) {return -1;}
    
    //检查当前目录下有无待删除目录
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
    }
    int j;
    for(j = 0; j < NUM_DENTRYS; j++){
        if(strcmp(path, dentrys[j].name) == 0 && dentrys[j].type != TYPE_FILE) break;
    }
    if(j == NUM_DENTRYS) {return -2;}
    
    //减少当前目录的硬链接数和used数
    inodess[jj].refer--;
    inodess[jj].used--;
    bios_sdwrite(kva2pa(inodess), 1, INODE_START_SEC + ii);
    
    //清除待删除目录在当前目录中的目录项
    int rm_inodeid = dentrys[j].inodeid;
    dentrys[j].inodeid = -1;
    dentrys[j].type = TYPE_NULL;
    dentrys[j].name[0] = '\0';
    for(int i = 0; i < 4; i++){
        bios_sdwrite(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
    }
    
    if(inodess[jj].refer != 0){return 0;}
    
    //重置数据块位图，不用清理数据块
    inode_t inodes[8];
    inode_t inode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + rm_inodeid / 8);
    inode = inodes[rm_inodeid % 8];
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(blockmap + 64*512*i), 64, BLKMAP_START_SEC + 64 * i);
    }
    for(int i = 0; i < 4; i++){
        blockmap[inode.ptr0[i] / 8] &= ~(1 << (7 - (inode.ptr0[i] % 8)));
    }
    for(int i = 0; i < 4; i++){
        bios_sdwrite(kva2pa(blockmap + 64*512*i), 64, BLKMAP_START_SEC + 64 * i);
    }
    
    //重置inode位图，不用清理inode
    char inodemapbuf[512];
    bios_sdread(kva2pa(inodemapbuf), 1, INODEMAP_START_SEC);
    inodemapbuf[rm_inodeid / 8] &= ~(1 << (7 - (rm_inodeid % 8)));
    bios_sdwrite(kva2pa(inodemapbuf), 1, INODEMAP_START_SEC);

    return 0;  // do_rmdir succeeds
}

int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    int cur_dir_inodeid = current_running->cur_dir_inodeid;
    inode_t inodess[NUM_INODES / 64];
    inode_t cur_dir_inode;
    int find = 0;
    int ii, jj;
    for(ii = 0; ii < 64; ii++){
        bios_sdread(kva2pa(inodess), 1, INODE_START_SEC + ii);
        for(jj = 0; jj < NUM_INODES / 64; jj++){
            if(cur_dir_inodeid == inodess[jj].inodeid){
                cur_dir_inode = inodess[jj];
                find = 1;
                goto here3;
            }
        }
    }
here3:
    if(!find) {return -1;}
    
    if(option == 0){
        screen_move_cursor(0, 0);
        for(int i = 0; i < 4; i++){
            bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
        }
        for(int i = 0; i < NUM_DENTRYS; i++){
            if((dentrys[i].type == TYPE_DIR) || (dentrys[i].type == TYPE_FILE)){
                printk("%s\n", dentrys[i].name);
            }
        }
    }
    
    else if(option == 1){
        screen_move_cursor(0, 0);
        for(int i = 0; i < 4; i++){
            bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
        }
        for(int i = 0; i < NUM_DENTRYS; i++){
            if((dentrys[i].type == TYPE_DIR) || (dentrys[i].type == TYPE_FILE)){
                printk("%s\t\t\t", dentrys[i].name);
                printk("inodeid: %d\t\t\t", dentrys[i].inodeid);
                inode_t inodes[8];
                inode_t inode;
                bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + dentrys[i].inodeid / 8);
                inode = inodes[dentrys[i].inodeid % 8];
                printk("links: %d\t\t\t", inode.refer);
                int hmnchars;
                if(dentrys[i].type == TYPE_DIR) {hmnchars = inode.used * 32; printk("occupying %d/%d chars\n", hmnchars, 4*4*1024);}
                else{
                    hmnchars = inode.used; printk("occupying %d chars\n", hmnchars);
                }
            }
        }
    }
    
    else assert(0);

    return 0;  // do_ls succeeds
}

int do_touch(char *path)
{
    // TODO [P6-task2]: Implement do_touch
    char zero[512];
    for(int i = 0; i < 512; i++){
        zero[i] = 0;
    }
    int cur_dir_inodeid = current_running->cur_dir_inodeid;
    inode_t inodess[NUM_INODES / 64];
    inode_t cur_dir_inode;
    int find = 0;
    int ii, jj;
    for(ii = 0; ii < 64; ii++){
        bios_sdread(kva2pa(inodess), 1, INODE_START_SEC + ii);
        for(jj = 0; jj < NUM_INODES / 64; jj++){
            if(cur_dir_inodeid == inodess[jj].inodeid){
                cur_dir_inode = inodess[jj];
                find = 1;
                goto here4;
            }
        }
    }
here4:
    if(!find) {return -1;}
    
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
    }
    int j;
    for(j = 0; j < NUM_DENTRYS; j++){
        if(strcmp(path, dentrys[j].name) == 0 && dentrys[j].type == TYPE_FILE) break;
    }
    if(j != NUM_DENTRYS) {return -2;}
    
    //used++
    inodess[jj].used++;
    bios_sdwrite(kva2pa(inodess), 1, INODE_START_SEC + ii);
    
    //分配目录项
    for(j = 0; j < NUM_DENTRYS; j++){
        if(dentrys[j].type == TYPE_NULL) break;
    }
    assert(j != NUM_DENTRYS);
    dentrys[j].type = TYPE_FILE;
    strcpy(dentrys[j].name, path);
    int create_inodeid;
    char inodemap[512];
    bios_sdread(kva2pa(inodemap), 1, INODEMAP_START_SEC);
    int k;
    //分配inode
    for(k = 0; k < 64; k++){
        if(inodemap[k] != 0xff) break;
    }
    assert(k != 64);
    int firstzero;
    char inodemapbuf = inodemap[k];
    for(int i = 0; i < 8; i++){
        if(!(inodemapbuf & 0x80)) {firstzero = i; break;}
        else{
            inodemapbuf = inodemapbuf << 1;
        }
    }
    create_inodeid = 8*k+firstzero; //注意位图的定义
    inodemap[k] |= (1 << (7-firstzero));
    bios_sdwrite(kva2pa(inodemap), 1, INODEMAP_START_SEC);
    
    dentrys[j].inodeid = create_inodeid; //父目录项初始化完成
    for(int i = 0; i < 4; i++){
        bios_sdwrite(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
    }
    
    //初始化inode
    inode_t inodes[8];
    inode_t inode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + create_inodeid / 8);  
    inode.inodeid = create_inodeid;
    inode.refer = 1;
    inode.mode = MODE_R | MODE_W;
    inode.used = 0;
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(blockmap + 64*512*i), 64, BLKMAP_START_SEC + 64 * i);
    }
    for(int i = 0; i < 7; i++){
        for(k = 0; k < 32*8*512; k++){
            if(blockmap[k] != 0xff) break;
        }
        assert(k != 32*8*512);
        char blkmapbuf = blockmap[k];
        for(int m = 0; m < 8; m++){
            if(!(blkmapbuf & 0x80)) {firstzero = m; break;}
            else blkmapbuf = blkmapbuf << 1;
        }
        inode.ptr0[i] = 8*k+firstzero;
        blockmap[k] |= (1 << (7-firstzero));
    }
    for(int i = 0; i < 2; i++){
        for(k = 0; k < 32*8*512; k++){
            if(blockmap[k] != 0xff) break;
        }
        assert(k != 32*8*512);
        char blkmapbuf = blockmap[k];
        for(int m = 0; m < 8; m++){
            if(!(blkmapbuf & 0x80)) {firstzero = m; break;}
            else blkmapbuf = blkmapbuf << 1;
        }
        inode.ptr1[i] = 8*k+firstzero;
        blockmap[k] |= (1 << (7-firstzero));
        
        uint32_t ptrs[128];
        for(int r = 0; r < 8; r++){
            for(int a = 0; a < 128; a++){
                for(k = 0; k < 32*8*512; k++){
                    if(blockmap[k] != 0xff) break;
                }
                assert(k != 32*8*512);
                char blkmapbuf = blockmap[k];
                for(int m = 0; m < 8; m++){
                    if(!(blkmapbuf & 0x80)) {firstzero = m; break;}
                    else blkmapbuf = blkmapbuf << 1;
                }
                ptrs[a] = 8 * k + firstzero;
                ptrs[a] |= (1 << 31);
                blockmap[k] |= (1 << (7-firstzero));
                
                for(int m = 0; m < 8; m++)
                    bios_sdwrite(kva2pa(zero), 1, DATA_START_SEC + 8 * (8 * k + firstzero) + m);
            }
            bios_sdwrite(kva2pa(ptrs), 1, DATA_START_SEC + inode.ptr1[i] * 8 + r);
        }
    }
    for(int i = 0; i < 4; i++)
        bios_sdwrite(kva2pa(blockmap + 64*512*i), 64, BLKMAP_START_SEC + 64 * i);
        
    for(int i = 0; i < 2; i++) inode.ptr2[i] = -1;  
    inodes[create_inodeid % 8] = inode;
    bios_sdwrite(kva2pa(inodes), 1, INODE_START_SEC + create_inodeid / 8);
    
    //没有义务给定文件内容的初始值。但方便起见还是清零

    for(int i = 0; i < 7; i++){
        for(int p = 0; p < 8; p++){
            bios_sdwrite(kva2pa(zero), 1, DATA_START_SEC + inode.ptr0[i] * 8 + p);
        }
    }
    
    return 0;  // do_touch succeeds
}

int do_cat(char *path)
{
    // TODO [P6-task2]: Implement do_cat
    //问题：怎么知道打印到哪里结束？
    //used成员现在指示文件的有效字节数。但还是不太确定到底该打印哪些内容，比如，将指针挪到正文后边很远的地方之后写信息，那么中间那段空白要不要打印？怎么确定哪些信息是该打印的？
    int cur_dir_inodeid = current_running->cur_dir_inodeid;
    inode_t inodess[NUM_INODES / 64];
    inode_t cur_dir_inode;
    int find = 0;
    int ii, jj;
    for(ii = 0; ii < 64; ii++){
        bios_sdread(kva2pa(inodess), 1, INODE_START_SEC + ii);
        for(jj = 0; jj < NUM_INODES / 64; jj++){
            if(cur_dir_inodeid == inodess[jj].inodeid){
                cur_dir_inode = inodess[jj];
                find = 1;
                goto here5;
            }
        }
    }
here5:
    if(!find) {return -1;}
    
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
    }
    int j;
    for(j = 0; j < NUM_DENTRYS; j++){
        if(strcmp(path, dentrys[j].name) == 0 && dentrys[j].type == TYPE_FILE) break;
    }
    if(j == NUM_DENTRYS) {return -2;}
    
    int cat_inodeid = dentrys[j].inodeid;
    inode_t inodes[8];
    inode_t inode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + cat_inodeid / 8);
    inode = inodes[cat_inodeid % 8];
    
    screen_move_cursor(0, 0);
    char buf[512];
    for(int i = 0; i < 7; i++){
        if(inode.ptr0[i] != -1){
            for(int k = 0; k < 8; k++){
                bios_sdread(kva2pa(buf), 1, DATA_START_SEC + inode.ptr0[i] * 8 + k);
                for(int m = 0; m < 512; m++) bios_putchar(buf[m]);
            }
        }
    }
    
    /*for(int i = 0; i < 3; i++){
        if(inode.ptr1[i] != -1){
            uint32_t ptrs[128];
            for(int k = 0; k < 8; k++){
                bios_sdread(kva2pa(ptrs), 1, DATA_START_SEC + inode.ptr1[i] * 8 + k);
                for(int m = 0; m < 128; m++){
                    if(ptrs[m] & (1 << 31)){
                        int blkno = ptrs[m] & (~(1 << 31));
                        for(int p = 0; p < 8; p++){
                            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + blkno * 8 + p);
                            for(int r = 0; r < 512; r++) bios_putchar(buf[r]);
                        }
                    }
                }
            }
        }
    }*/ //阉割版

    return 0;  // do_cat succeeds
}

int do_fopen(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_fopen
    if(mode != O_RDONLY && mode != O_WRONLY && mode != O_RDWR) return -1;
    int cur_dir_inodeid = current_running->cur_dir_inodeid;
    inode_t inodess[NUM_INODES / 64];
    inode_t cur_dir_inode;
    int find = 0;
    int ii, jj;
    for(ii = 0; ii < 64; ii++){
        bios_sdread(kva2pa(inodess), 1, INODE_START_SEC + ii);
        for(jj = 0; jj < NUM_INODES / 64; jj++){
            if(cur_dir_inodeid == inodess[jj].inodeid){
                cur_dir_inode = inodess[jj];
                find = 1;
                goto here7;
            }
        }
    }
here7:
    if(!find) {return -1;}
    
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + cur_dir_inode.ptr0[i] * 8);
    }
    int j;
    for(j = 0; j < NUM_DENTRYS; j++){
        if(strcmp(path, dentrys[j].name) == 0 && dentrys[j].type == TYPE_FILE) break;
    }
    if(j == NUM_DENTRYS) {return -2;}
    
    int open_inodeid = dentrys[j].inodeid;
    
    int i;
    for(i = 0; i < NUM_FDESCS; i++){
        if(!fdesc_array[i].used){
            fdesc_array[i].used = 1;
            fdesc_array[i].inodeid = open_inodeid;
            fdesc_array[i].mode = mode;
            fdesc_array[i].rpos = 0;
            fdesc_array[i].wpos = 0;
            break;
        }
    }
    assert(i != NUM_FDESCS);

    return i;  // return the id of file descriptor
}

//创建文件时，先为其分配7个数据块。当需要扩充文件大小时，会产生7块之外地址的读或写操作。对于未分配地址的读操作，设计成报错；对于未分配地址的写操作，利用一级间址ptr1分配足够的空间后进行写。注意分配空间后要保证访问地址之前的所有地址也都分配了空间。
int do_fread(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fread
    int inodeid = fdesc_array[fd].inodeid;
    int rpos = fdesc_array[fd].rpos;
    if(fdesc_array[fd].mode != O_RDONLY && fdesc_array[fd].mode != O_RDWR) return -2;
    inode_t inodes[8];
    inode_t inode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + inodeid / 8);
    inode = inodes[inodeid % 8];
    if(rpos <= 7 * 4 * 1024 - length){
        int whichblk = inode.ptr0[rpos / 4096];
        //方便起见此处不支持读大于512字节。
        assert(length <= 512);
        int offsetinblk = rpos % 4096;
        char buf[512];
        if((offsetinblk % 512) + length <= 512){
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * whichblk + offsetinblk / 512);
            strncpy(buff, buf + (offsetinblk % 512), length);
            fdesc_array[fd].rpos += length;
            return length;
        }
        else{ //有可能跨越block，没处理
            assert(offsetinblk / 512 != 7);
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * whichblk + offsetinblk / 512);
            strncpy(buff, buf + (offsetinblk % 512), 512 - (offsetinblk % 512));
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * whichblk + offsetinblk / 512 + 1);
            strncpy(buff + 512 - (offsetinblk % 512), buf, length - (512 - (offsetinblk % 512)));
            fdesc_array[fd].rpos += length;
            return length;
        }
    }
    else{
        int whichptr1 = rpos / (4 * 1024 * 1024);
        if((rpos % (4 * 1024 * 1024) < 7 * 4 * 1024) && (whichptr1 > 0)) whichptr1 -= 1;
        if(inode.ptr1[whichptr1] == -1){
            screen_move_cursor(0, 0);
            printk("read out of range\n");
            return -1;
        }
        int blkinptr1 = (rpos - 7 * 4 * 1024 - whichptr1 * 4 * 1024 * 1024) / (4 * 1024);
        uint32_t ptrs[128];
        bios_sdread(kva2pa(ptrs), 1, DATA_START_SEC + inode.ptr1[whichptr1] * 8 + blkinptr1 / 128);
        uint32_t finalptr = ptrs[blkinptr1 % 128];
        if(!(finalptr & (1 << 31))){
            screen_move_cursor(0, 0);
            printk("read out of range\n");
            return -1;
        }
        int finalblk = finalptr & (~(1 << 31));
        
        int offsetinblk = rpos % 4096;
        char buf[512];
        if((offsetinblk % 512) + length <= 512){
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * finalblk + offsetinblk / 512);
            strncpy(buff, buf + (offsetinblk % 512), length);
            fdesc_array[fd].rpos += length;
            return length;
        }
        else{ //同上
            assert(offsetinblk / 512 != 7);
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * finalblk + offsetinblk / 512);
            strncpy(buff, buf + (offsetinblk % 512), 512 - (offsetinblk % 512));
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * finalblk + offsetinblk / 512 + 1);
            strncpy(buff + 512 - (offsetinblk % 512), buf, length - (512 - (offsetinblk % 512)));
            fdesc_array[fd].rpos += length;
            return length;
        }
        
    }

    return 0;  // return the length of trully read data
}

int do_fwrite(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fwrite
    int inodeid = fdesc_array[fd].inodeid;
    int wpos = fdesc_array[fd].wpos;
    if(fdesc_array[fd].mode != O_WRONLY && fdesc_array[fd].mode != O_RDWR) return -2;
    inode_t inodes[8];
    inode_t inode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + inodeid / 8);
    inode = inodes[inodeid % 8];
    if(wpos > inode.used){
        inode.used = wpos;
        inodes[inodeid % 8] = inode;
        bios_sdwrite(kva2pa(inodes), 1, INODE_START_SEC + inodeid / 8);
    }
    
    if(wpos <= 7 * 4 * 1024 - length){
        int whichblk = inode.ptr0[wpos / 4096];
        //方便起见此处不支持读大于512字节。
        assert(length <= 512);
        int offsetinblk = wpos % 4096;
        char buf[512];
        if((offsetinblk % 512) + length <= 512){
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * whichblk + offsetinblk / 512);
            strncpy(buf + (offsetinblk % 512), buff, length);
            bios_sdwrite(kva2pa(buf), 1, DATA_START_SEC + 8 * whichblk + offsetinblk / 512);
            fdesc_array[fd].wpos += length;
            return length;
        }
        else{ //有可能跨越block，没处理
            assert(offsetinblk / 512 != 7);
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * whichblk + offsetinblk / 512);
            strncpy(buf + (offsetinblk % 512), buff, 512 - (offsetinblk % 512));
            bios_sdwrite(kva2pa(buf), 1, DATA_START_SEC + 8 * whichblk + offsetinblk / 512);
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * whichblk + offsetinblk / 512 + 1);
            strncpy(buf, buff + 512 - (offsetinblk % 512), length - (512 - (offsetinblk % 512)));
            bios_sdwrite(kva2pa(buf), 1, DATA_START_SEC + 8 * whichblk + offsetinblk / 512 + 1);
            fdesc_array[fd].wpos += length;
            return length;
        }
    }
    else{
        int whichptr1 = wpos / (4 * 1024 * 1024);
        if((wpos % (4 * 1024 * 1024) < 7 * 4 * 1024) && (whichptr1 > 0)) whichptr1 -= 1;
        /*if(inode.ptr1[whichptr1] == -1){
            for(int i = 0; i < 4; i++){
                bios_sdread(kva2pa(blockmap + 64*512*i), 64, BLKMAP_START_SEC + 64 * i);
            }
            for(int k = 0; k < 32*8*512; k++){
                if(blockmap[k] != 0xff) break;
            }
            assert(k != 32*8*512);
            char blkmapbuf = blockmap[k];
            int firstzero;
            for(int m = 0; m < 8; m++){
                if(!(blkmapbuf & 0x80)) {firstzero = m; break;}
                else blkmapbuf = blkmapbuf << 1;
            }
            inode.ptr1[whichptr1] = 8*k+firstzero;
            blockmap[k] |= (1 << (7-firstzero));
            for(int i = 0; i < 4; i++){
                bios_sdwrite(kva2pa(blockmap + 64*512*i), 64, BLKMAP_START_SEC + 64 * i);
            }
            
            uint32_t bufff[128];
            for(int i = 0; i < 128; i++) bufff[i] = 0;
            for(int i = 0; i < 8; i++){
                bios_sdwrite(kva2pa(bufff), 1, DATA_START_SEC + 8 * inode.ptr1[whichptr1] + i);
            }
        }*/
        int blkinptr1 = (wpos - 7 * 4 * 1024 - whichptr1 * 4 * 1024 * 1024) / (4 * 1024);
        uint32_t ptrs[128];
        bios_sdread(kva2pa(ptrs), 1, DATA_START_SEC + inode.ptr1[whichptr1] * 8 + blkinptr1 / 128);
        uint32_t finalptr = ptrs[blkinptr1 % 128];
        /*if(!(finalptr & (1 << 31))){
            if(whichptr1 == 0){
                int used = 0;
                for(int i = 0; i < 8; i++){
                    bios_sdread(kva2pa(ptrs), 1, DATA_START_SEC + inode.ptr1[whichptr1] * 8 + i);
                    for(int j = 0; j < 128; j++){
                        if(ptrs[j] & (1 << 31)) used++;
                        else goto out;
                    }
                }
out:
                int needfill = blkinptr1 - used;
                for(int i = 0; i < needfill; i++){
                    
                }
            }
        }*/
        int finalblk = finalptr & (~(1 << 31));
        
        int offsetinblk = wpos % 4096;
        char buf[512];
        if((offsetinblk % 512) + length <= 512){
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * finalblk + offsetinblk / 512);
            strncpy(buf + (offsetinblk % 512), buff, length);
            fdesc_array[fd].wpos += length;
            bios_sdwrite(kva2pa(buf), 1, DATA_START_SEC + 8 * finalblk + offsetinblk / 512);
            return length;
        }
        else{ //同上
            assert(offsetinblk / 512 != 7);
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * finalblk + offsetinblk / 512);
            strncpy(buf + (offsetinblk % 512), buff, 512 - (offsetinblk % 512));
            bios_sdwrite(kva2pa(buf), 1, DATA_START_SEC + 8 * finalblk + offsetinblk / 512);
            bios_sdread(kva2pa(buf), 1, DATA_START_SEC + 8 * finalblk + offsetinblk / 512 + 1);
            strncpy(buf, buff + 512 - (offsetinblk % 512), length - (512 - (offsetinblk % 512)));
            bios_sdwrite(kva2pa(buf), 1, DATA_START_SEC + 8 * finalblk + offsetinblk / 512 + 1);
            fdesc_array[fd].wpos += length;
            return length;
        }
        
    }

    return 0;  // return the length of trully written data
}

int do_fclose(int fd)
{
    // TODO [P6-task2]: Implement do_fclose
    assert(fd >= 0 && fd < NUM_FDESCS);
    fdesc_array[fd].used = 0;

    return 0;  // do_fclose succeeds
}

int do_ln(char *src_path, char *dst_path) //认为src是目录，dst是文件
{
    // TODO [P6-task2]: Implement do_ln
    //考虑先cd到目标的上级目录（最后一个/前边的目录），之后读出必要信息后再把current_running->cur_dir_inodeid改回原值。记得cd时第二个参数输入0避免打印。
    //可以先cd到目标文件的父目录，找到文件inode后给refer++，再回到原目录，再cd到src目录的父目录，填好src目录目录项后再回到原目录结束
    int cur_dir_inodeid = current_running->cur_dir_inodeid;
    int file_ancdir_inodeid = cur_dir_inodeid;
    int p;
    for(p = strlen(dst_path) - 1; p >= 0; p--)
        if(dst_path[p] == '/') break;
    if(p != -1){
        char ancpath[30];
        for(int i = 0; i < p; i++) ancpath[i] = dst_path[i];
        ancpath[p] = '\0';
        if(do_cd(ancpath, 0) != 0) return -1;
        file_ancdir_inodeid = current_running->cur_dir_inodeid;
        current_running->cur_dir_inodeid = cur_dir_inodeid;
    }
    char filename[30];
    for(int i = p + 1; i <= strlen(dst_path); i++) filename[i - (p + 1)] = dst_path[i];
    inode_t inodes[8];
    inode_t fadinode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + file_ancdir_inodeid / 8);
    fadinode = inodes[file_ancdir_inodeid % 8];
    
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + fadinode.ptr0[i] * 8);
    }
    int j;
    for(j = 0; j < NUM_DENTRYS; j++){
        if(strcmp(filename, dentrys[j].name) == 0 && dentrys[j].type == TYPE_FILE) break;
    }
    if(j == NUM_DENTRYS) {return -2;}
    int fileinodeid = dentrys[j].inodeid;
    
    //来到src目录的父目录
    int src_dir_ancid = current_running->cur_dir_inodeid;
    for(p = strlen(src_path) - 1; p >= 0; p--)
        if(src_path[p] == '/') break;
    if(p != -1){
        char ancpath1[30];
        for(int i = 0; i < p; i++) ancpath1[i] = src_path[i];
        ancpath1[p] = '\0';
        if(do_cd(ancpath1, 0) != 0) return -3;
        src_dir_ancid = current_running->cur_dir_inodeid;
        current_running->cur_dir_inodeid = cur_dir_inodeid;
    }
    inode_t dadinode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + src_dir_ancid / 8);
    dadinode = inodes[src_dir_ancid % 8];
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + dadinode.ptr0[i] * 8);
    }
    char dirname[30];
    for(int i = p + 1; i < strlen(src_path); i++) dirname[i - (p + 1)] = src_path[i];
    for(j = 0; j < NUM_DENTRYS; j++){
        if(strcmp(dirname, dentrys[j].name) == 0 && dentrys[j].type != TYPE_FILE) break;
    }
    if(j == NUM_DENTRYS) {return -4;}
    int dirinodeid = dentrys[j].inodeid;
    
    //修改源目录，添加目标文件
    inode_t dirinode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + dirinodeid / 8);
    dirinode = inodes[dirinodeid % 8];
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + dirinode.ptr0[i] * 8);
    }
    int k;
    for(k = 0; k < NUM_DENTRYS; k++){
        if(dentrys[k].type == TYPE_NULL) break;
    }
    assert(k != NUM_DENTRYS);
    dentrys[k].inodeid = fileinodeid;
    dentrys[k].type = TYPE_FILE;
    strcpy(dentrys[k].name, filename);
    for(int i = 0; i < 4; i++){
        bios_sdwrite(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + dirinode.ptr0[i] * 8);
    }
    
    //修改文件inode，增加引用数
    inode_t finode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + fileinodeid / 8);
    finode = inodes[fileinodeid % 8];
    finode.refer++;
    inodes[fileinodeid % 8] = finode;
    bios_sdwrite(kva2pa(inodes), 1, INODE_START_SEC + fileinodeid / 8);
    

    return 0;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm
    int cur_dir_inodeid = current_running->cur_dir_inodeid;
    int file_ancdir_inodeid = cur_dir_inodeid;
    int p;
    for(p = strlen(path) - 1; p >= 0; p--)
        if(path[p] == '/') break;
    if(p != -1){
        char ancpath[30];
        for(int i = 0; i < p; i++) ancpath[i] = path[i];
        ancpath[p] = '\0';
        if(do_cd(ancpath, 0) != 0) return -1;
        file_ancdir_inodeid = current_running->cur_dir_inodeid;
        current_running->cur_dir_inodeid = cur_dir_inodeid;
    }
    char filename[30];
    for(int i = p + 1; i <= strlen(path); i++) filename[i - (p + 1)] = path[i];
    inode_t inodes[8];
    inode_t fadinode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + file_ancdir_inodeid / 8);
    fadinode = inodes[file_ancdir_inodeid % 8];
    fadinode.used--;
    bios_sdwrite(kva2pa(inodes), 1, INODE_START_SEC + file_ancdir_inodeid / 8);
    
    for(int i = 0; i < 4; i++){
        bios_sdread(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + fadinode.ptr0[i] * 8);
    }
    int j;
    for(j = 0; j < NUM_DENTRYS; j++){
        if(strcmp(filename, dentrys[j].name) == 0 && dentrys[j].type == TYPE_FILE) break;
    }
    if(j == NUM_DENTRYS) {return -2;}
    int fileinodeid = dentrys[j].inodeid;
    inode_t finode;
    bios_sdread(kva2pa(inodes), 1, INODE_START_SEC + fileinodeid / 8);
    finode = inodes[fileinodeid % 8];
    finode.refer--;
    if(finode.refer){
        inodes[fileinodeid % 8] = finode;
        bios_sdwrite(kva2pa(inodes), 1, INODE_START_SEC + fileinodeid / 8);
    }
    else{
        finode.inodeid = -1;
        finode.used = 0;
        
        for(int m = 0; m < 4; m++){
            bios_sdread(kva2pa(blockmap + 64*512*m), 64, BLKMAP_START_SEC + 64 * m);
        }
        for(int i = 0; i < 7; i++){
            int blkid = finode.ptr0[i];
            blockmap[blkid / 8] &= ~(1 << (7 - (blkid % 8)));
        }
        for(int i = 0; i < 2; i++){
            uint32_t map[128];
            for(int k = 0; k < 8; k++){
                bios_sdread(kva2pa(map), 1, DATA_START_SEC + finode.ptr1[i] * 8 + k);
                for(int m = 0; m < 128; m++){
                    if(map[m] & (1 << 31)){
                        map[m] &= ~(1 << 31);
                        blockmap[map[m] / 8] &= ~(1 << (7 - (map[m] % 8)));
                    }
                }
            }
            blockmap[finode.ptr1[i] / 8] &= ~(1 << (7 - (finode.ptr1[i] % 8)));
        }
        for(int m = 0; m < 4; m++){
            bios_sdwrite(kva2pa(blockmap + 64*512*m), 64, BLKMAP_START_SEC + 64 * m);
        }
        
        for(int i = 0; i < 7; i++) finode.ptr0[i] = -1;
        for(int i = 0; i < 3; i++) finode.ptr1[i] = -1;
        bios_sdwrite(kva2pa(inodes), 1, INODE_START_SEC + fileinodeid / 8);
        
        char imap[512];
        bios_sdread(kva2pa(imap), 1, INODEMAP_START_SEC);
        imap[fileinodeid / 8] &= ~(1 << (7 - (fileinodeid % 8)));
        bios_sdwrite(kva2pa(imap), 1, INODEMAP_START_SEC);
    }
    dentrys[j].inodeid = -1;
    dentrys[j].type = TYPE_NULL;
    dentrys[j].name[0] = '\0';
    for(int i = 0; i < 4; i++){
        bios_sdwrite(kva2pa(dentrys + i * NUM_DENTRYS / 4), 8, DATA_START_SEC + fadinode.ptr0[i] * 8);
    }

    return 0;  // do_rm succeeds 
}

int do_lseek(int fd, int offset)
{
    // TODO [P6-task2]: Implement do_lseek
    assert(fd >= 0 && fd < NUM_FDESCS);
    assert(offset >= 0 && offset < 8 * 1024 * 1024);
    fdesc_array[fd].rpos = fdesc_array[fd].wpos = offset;

    return offset;  // the resulting offset location from the beginning of the file
}
