/**
 * File              : sha256.h
 *
 * Authors           : Fabio Scatozza      <s315216@studenti.polito.it>
 *                     Isacco Delpero      <s314713@studenti.polito.it>
 *                     Leonardo Cerruti    <s317664@studenti.polito.it>
 *                     Claudio Capobianchi <s319187@studenti.polito.it>
 *                     Saman Alipour       <s307980@studenti.polito.it>
 *                     Abdul Rehman        <s315198@studenti.polito.it>
 *                     Stephano Perera     <s313080@studenti.polito.it>
 *
 * Date              : 07.07.2023
 * Last Modified Date: 20.07.2023
 *
 * Copyright (c) 2023
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SHA256_H
#define SHA256_H

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>

/*
 * descriptive macros
 */
#define DEVICE_NAME "sha256"
#define DRIVER_NAME DEVICE_NAME
#define CLASS_NAME  "crypto"

/*
 * module parameters
 */
static bool irq_enable;        /* load time parameter to choose irq or polling */

/*
 * utility macros
 */
#define PR_ALERT(fmt, args...) pr_alert(DRIVER_NAME ": " fmt "\n", ## args)
#define PR_INFO(fmt, args...) pr_info(DRIVER_NAME ": " fmt "\n", ## args)
#define PR_DEVEL(fmt, args...) pr_devel(DRIVER_NAME ": " fmt "\n", ## args)

/*
 * device definitions
 */
struct sha256_mmap {
  u32 CSR; /* control and status register */

  u32 W0;  /* most significant message block word */
  u32 W1;
  u32 W2;
  u32 W3;
  u32 W4;
  u32 W5;
  u32 W6;
  u32 W7;
  u32 W8;
  u32 W9;
  u32 W10;
  u32 W11;
  u32 W12;
  u32 W13;
  u32 W14;
  u32 W15;

  u32 H7;  /* most significant hash word */
  u32 H6;
  u32 H5;
  u32 H4;
  u32 H3;
  u32 H2;
  u32 H1;
  u32 H0;
};

#define SHA256_MMAP_ERR           (0x1 << 31)
#define SHA256_MMAP_DONE          (0x1 << 30)
#define SHA256_MMAP_BLOCK_READ    (0x1 << 29)
#define SHA256_MMAP_IENABLE       (0x1 << 10)
#define SHA256_MMAP_WR            (0x1 << 9)
#define SHA256_MMAP_LAST          (0x1 << 8)
#define SHA256_MMAP_NEWHASH       (0x1 << 1)
#define SHA256_MMAP_COREENABLE    (0x1)

#define SHA256_MMAP_BASECSR       ((irq_enable ? SHA256_MMAP_IENABLE : 0) | SHA256_MMAP_COREENABLE)

#define SHA256_IOREAD32(mmap_addr, reg)\
  ioread32((mmap_addr) + offsetof(struct sha256_mmap, reg))

#define SHA256_IOWRITE32_RAW(value, mmap_addr, reg) \
  iowrite32((value), (mmap_addr) + offsetof(struct sha256_mmap, reg))

#ifdef DEBUG_VERBOSE
#  define SHA256_IOWRITE32(value, mmap_addr, reg) \
  do { \
    PRINTD("testing write operation..."); \
    PRINTD("write: 0x%08X", value); \
    SHA256_IOWRITE32_RAW(value, mmap_addr, reg); \
    PRINTD("read: 0x%08X", SHA256_IOREAD32(mmap_addr, reg)); \
    } while (0);
#else
#  define SHA256_IOWRITE32(value, mmap_addr, reg) SHA256_IOWRITE32_RAW(value, mmap_addr, reg)
#endif

#define POLLING_SLEEP_US          1 /* roughly 1 block processing delay */

/*
 * device representation
 */
struct sha256_dev {

  /* get resources when probing the device tree */
  int irq;
  unsigned long mem_start;
  unsigned long mem_end;
  void __iomem *base_addr;

  /* implementation details */
  atomic_t open_cnt;                /* open() counter */
  unsigned long skip_core_enable;   /* core enable initialization */
  struct semaphore sem;             /* device big lock */

  bool msg_concat;                  /* message concatenation flag */
  u8 dev_bytes;                     /* valid bytes in the device: [0:64] */

  bool pending_seek;                /* file position change requested */
  loff_t cur_fpos;                  /* actual file position */
  loff_t new_fpos;                  /* new file position due to a pending seek */

  wait_queue_head_t wr_q;           /* write queue */
  wait_queue_head_t rd_q;           /* read queue */

  void *hash_ptr;                   /* buffered hash */
  void *block_ptr;                  /* buffered block */

  struct cdev cdev;                 /* kernel character device representation */
};

/*
 * ioctl definitions
 */
#define SHA256_IOC_MAGIC  'S'
#define SHA256_IOC_WCAT   _IOW(SHA256_IOC_MAGIC, 1, int)
#define SHA256_IOC_MAXNR  1

/*
 * file operations
 */
static loff_t sha256Llseek(struct file *filp, loff_t off, int whence);
static ssize_t sha256Read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t sha256Write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static long sha256Ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
static int sha256Open (struct inode *inode, struct file *filp);
static int sha256Release (struct inode *inode, struct file *filp);
static int sha256CheckFlags(int flags);

/*
 * private operations
 */
static bool deviceWrite(const char *buf, size_t count);
static bool deviceLast(void);

#endif //GV_SHA256_H

