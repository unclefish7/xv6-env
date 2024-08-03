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
