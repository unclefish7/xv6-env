# Lab: File System

## 实验报告：Large File 实验

### 实验目的

本实验的主要目的是通过实现间接块和双重间接块的功能，扩展 xv6 文件系统的文件大小限制，使其能够支持大于 140 个扇区（约 70KB）的文件。

### 实验步骤

#### 修改 inode 结构

在 `fs.h` 中的 `struct dinode` 中添加一个间接块指针和一个双重间接块指针。

```c
#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)

struct dinode {
  short type;
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+2]; // +1 for single indirect, +1 for double indirect
};
```

#### 实现间接块功能

在 `fs.c` 中的 `bmap` 函数中处理间接块和双重间接块。

```c
uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if (bn < NDIRECT) {
    if ((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if (bn < NINDIRECT) {
    if ((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if ((addr = a[bn]) == 0) {
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  bn -= NINDIRECT;

  if (bn < NINDIRECT * NINDIRECT) {
    if ((addr = ip->addrs[NDIRECT+1]) == 0)
      ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    uint bn1 = bn / NINDIRECT;
    uint bn2 = bn % NINDIRECT;
    if ((addr = a[bn1]) == 0) {
      a[bn1] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if ((addr = a[bn2]) == 0) {
      a[bn2] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}
```

#### 修改 itrunc 函数

在 `fs.c` 中修改 `itrunc` 函数，处理间接块和双重间接块的释放。

```c
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  if(ip->addrs[NDIRECT+1]){
    bp = bread(ip->dev, ip->addrs[NDIRECT+1]);
    a = (uint*)bp->data;
    for(i = 0; i < NINDIRECT; i++){
      if(a[i]){
        struct buf *bp2 = bread(ip->dev, a[i]);
        uint *a2 = (uint*)bp2->data;
        for(j = 0; j < NINDIRECT; j++){
          if(a2[j])
            bfree(ip->dev, a2[j]);
        }
        brelse(bp2);
        bfree(ip->dev, a[i]);
      }
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT+1]);
    ip->addrs[NDIRECT+1] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}
```

### 遇到的问题及解决方法

#### 问题一：地址计算错误

在处理双重间接块时，容易出现地址计算错误，需要仔细检查 `bmap` 函数中双重间接块的地址计算。

#### 问题二：内存释放不完整

在 `itrunc` 函数中，需要确保释放所有间接块和双重间接块，防止内存泄漏。

### 总结与体会

通过本实验，我们学习了如何扩展 xv6 文件系统以支持大文件，深入理解了文件系统中间接块和双重间接块的原理和实现方法。在实验过程中，遇到了地址计算和内存释放的问题，通过调试和仔细分析代码，成功解决了这些问题。此次实验不仅巩固了我们对文件系统的理解，也提高了我们的编程调试能力。


## Symbolic Links 实验报告

### 实验目的

实现符号链接（symbolic links），并深入理解路径解析的工作原理。符号链接指向一个文件路径，当打开符号链接时，内核会跟随链接指向的目标文件。

### 实验步骤

1. **添加系统调用**：在 `kernel/syscall.h` 中添加 `SYS_symlink`。

   ```c
   #define SYS_symlink 22
   ```
2. **更新用户接口**：在 `user/user.h` 中添加 `symlink` 函数声明。

   ```c
   int symlink(const char *target, const char *path);
   ```
3. **实现系统调用**：
   在 `kernel/sysfile.c` 中实现 `sys_symlink` 函数。

   ```c
   int
   sys_symlink(void)
   {
     char target[MAXPATH], path[MAXPATH];
     struct inode *ip;

     if(argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0)
       return -1;

     begin_op();
     if((ip = create(path, T_SYMLINK, 0, 0)) == 0){
       end_op();
       return -1;
     }
     if(writei(ip, 0, (uint64)target, 0, strlen(target)) != strlen(target)){
       iunlockput(ip);
       end_op();
       return -1;
     }
     iupdate(ip);
     iunlockput(ip);
     end_op();
     return 0;
   }
   ```
4. **添加文件类型**：在 `kernel/stat.h` 中添加符号链接类型。

   ```c
   #define T_SYMLINK 3
   ```
5. **更新 `fcntl.h`**：添加 `O_NOFOLLOW` 标志。

   ```c
   #define O_NOFOLLOW 0x100
   ```
6. **修改 `sys_open`**：处理符号链接解析。

   ```c
   uint64
   sys_open(void)
   {
     char path[MAXPATH];
     int fd, omode;
     struct file *f;
     struct inode *ip;
     int n, deref = 0;

     if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
       return -1;

     begin_op();

     if(omode & O_CREATE){
       ip = create(path, T_FILE, 0, 0);
       if(ip == 0){
         end_op();
         return -1;
       }
     } else {
       if((ip = namei(path)) == 0){
         end_op();
         return -1;
       }
       ilock(ip);
       if(ip->type == T_DIR && omode != O_RDONLY){
         iunlockput(ip);
         end_op();
         return -1;
       }

       while(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW) && deref < 10) {
         char sym_path[MAXPATH];
         if (readi(ip, 0, (uint64)sym_path, 0, MAXPATH) < 0) {
           iunlockput(ip);
           end_op();
           return -1;
         }
         sym_path[MAXPATH-1] = '\0';
         iunlockput(ip);
         if ((ip = namei(sym_path)) == 0) {
           end_op();
           return -1;
         }
         ilock(ip);
         deref++;
       }

       if(deref == 10) {
         iunlockput(ip);
         end_op();
         return -1;
       }
     }

     if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
       iunlockput(ip);
       end_op();
       return -1;
     }

     if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
       if(f)
         fileclose(f);
       iunlockput(ip);
       end_op();
       return -1;
     }

     if(ip->type == T_DEVICE){
       f->type = FD_DEVICE;
       f->major = ip->major;
     } else {
       f->type = FD_INODE;
       f->off = 0;
     }
     f->ip = ip;
     f->readable = !(omode & O_WRONLY);
     f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

     if((omode & O_TRUNC) && ip->type == T_FILE){
       itrunc(ip);
     }

     iunlock(ip);
     end_op();
     return fd;
   }
   ```
7. **测试**：添加并运行 `symlinktest` 验证符号链接的实现。

### 遇到的问题及解决方法

1. **磁盘中断处理错误**：
   - **问题描述**：在运行 `symlinktest` 时，遇到 `virtio_disk_intr status` panic 错误。
   - **调试步骤**：
     - 在 `virtio_disk_intr` 和相关函数中添加调试信息。
     - 检查磁盘操作和中断处理的一致性。
   - **现状**：错误尚未解决，怀疑与磁盘操作或符号链接处理有关。

### 总结与体会

通过该实验，深入理解了符号链接的实现和路径解析机制。尽管遇到了磁盘中断处理的挑战，但通过详细的调试和分析，增强了对操作系统内部机制的理解。后续需要进一步解决磁盘中断处理错误，确保符号链接功能的稳定性。
