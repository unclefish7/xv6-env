#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// 记录 e1000 寄存器的基地址
static volatile uint32 *regs;

struct spinlock e1000_lock;

// 由 pci_init() 调用，初始化 e1000
void
e1000_init(uint32 *xregs)
{
  initlock(&e1000_lock, "e1000"); // 初始化锁，用于同步访问 E1000 设备

  regs = xregs; // 保存寄存器基地址

  // 重置设备
  regs[E1000_IMS] = 0; // 禁用中断
  regs[E1000_CTL] |= E1000_CTL_RST; // 设置复位位
  regs[E1000_IMS] = 0; // 重新禁用中断
  __sync_synchronize(); // 内存屏障，确保所有之前的写操作完成

  // 发送环初始化
  memset(tx_ring, 0, sizeof(tx_ring)); // 清空发送环
  for (int i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD; // 初始化每个描述符的状态为已完成
    tx_mbufs[i] = 0; // 初始化 mbuf 指针
  }
  regs[E1000_TDBAL] = (uint64) tx_ring; // 设置发送环基地址
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring); // 设置发送环长度
  regs[E1000_TDH] = regs[E1000_TDT] = 0; // 初始化发送环头尾指针
  
  // 接收环初始化
  memset(rx_ring, 0, sizeof(rx_ring)); // 清空接收环
  for (int i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0); // 分配 mbuf
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head; // 设置描述符地址
  }
  regs[E1000_RDBAL] = (uint64) rx_ring; // 设置接收环基地址
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0; // 初始化接收环头指针
  regs[E1000_RDT] = RX_RING_SIZE - 1; // 初始化接收环尾指针

  // 设置 MAC 地址过滤器，设置 QEMU 默认的 MAC 地址
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);

  // 设置组播表
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // 设置发送控制寄存器
  regs[E1000_TCTL] = E1000_TCTL_EN | // 启用发送
    E1000_TCTL_PSP | // 短包填充
    (0x10 << E1000_TCTL_CT_SHIFT) | // 冲突阈值
    (0x40 << E1000_TCTL_COLD_SHIFT); // 冲突距离
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // 设置发送间隔

  // 设置接收控制寄存器
  regs[E1000_RCTL] = E1000_RCTL_EN | // 启用接收
    E1000_RCTL_BAM | // 启用广播
    E1000_RCTL_SZ_2048 | // 2048 字节接收缓冲区
    E1000_RCTL_SECRC; // 去掉 CRC

  // 启用接收中断
  regs[E1000_RDTR] = 0; // 每接收一个包触发中断（无定时器）
  regs[E1000_RADV] = 0; // 每接收一个包触发中断（无定时器）
  regs[E1000_IMS] = (1 << 7); // 启用接收描述符写回中断
}

// 实现发送数据包的函数
int e1000_transmit(struct mbuf *m)
{
  acquire(&e1000_lock); // 获取锁，保护共享资源

  uint32 index = regs[E1000_TDT]; // 获取发送环尾指针
  struct tx_desc *desc = &tx_ring[index]; // 获取当前描述符

  // 检查描述符是否可用
  if (!(desc->status & E1000_TXD_STAT_DD)) {
    release(&e1000_lock); // 释放锁
    return -1; // 发送队列已满
  }

  // 释放之前的 mbuf
  if (tx_mbufs[index]) {
    mbuffree(tx_mbufs[index]);
  }

  // 设置新的描述符
  desc->addr = (uint64)m->head; // 设置数据包地址
  desc->length = m->len; // 设置数据包长度
  desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS; // 设置命令位
  desc->status = 0; // 清除状态位

  tx_mbufs[index] = m; // 保存当前 mbuf

  // 更新发送环尾指针
  regs[E1000_TDT] = (index + 1) % TX_RING_SIZE;

  release(&e1000_lock); // 释放锁
  return 0;
}

// 实现接收数据包的函数
static void e1000_recv(void)
{
  acquire(&e1000_lock); // 获取锁，保护共享资源

  while (1) {
    uint32 index = (regs[E1000_RDT] + 1) % RX_RING_SIZE; // 获取接收环尾指针的下一个索引
    struct rx_desc *desc = &rx_ring[index]; // 获取当前描述符

    // 检查描述符是否已被硬件处理
    if (!(desc->status & E1000_RXD_STAT_DD)) {
      break; // 没有新包，退出循环
    }

    // 检查包是否完整
    if (!(desc->status & E1000_RXD_STAT_EOP)) {
      panic("e1000_recv not EOP"); // 包未完成
    }

    struct mbuf *m = rx_mbufs[index]; // 获取当前 mbuf
    m->len = desc->length; // 设置数据包长度

    net_rx(m); // 交由网络层处理

    // 分配新的 mbuf 替换已使用的 mbuf
    struct mbuf *new_mbuf = mbufalloc(0);
    if (!new_mbuf) {
      panic("e1000_recv mbufalloc");
    }

    desc->addr = (uint64)new_mbuf->head; // 更新描述符地址
    desc->status = 0; // 清除状态位
    rx_mbufs[index] = new_mbuf; // 更新 mbuf

    regs[E1000_RDT] = index; // 更新接收环尾指针
  }

  release(&e1000_lock); // 释放锁
}

// 实现中断处理函数
void e1000_intr(void)
{
  regs[E1000_ICR] = 0xffffffff; // 清除中断标志，允许新的中断

  e1000_recv(); // 处理接收的数据包
}
