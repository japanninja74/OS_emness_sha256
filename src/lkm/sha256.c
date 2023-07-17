/**
 * File              : sha256.c
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
 * Last Modified Date: 11.07.2023
 *
 * Copyright (c) 2023
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include <linux/init.h>         /* initialization & cleanup */
#include <linux/sched.h>        /* kernel api */
#include <linux/module.h>       /* core header for LKMs */
#include <linux/moduleparam.h>  /* support for load-time parameters */
#include <linux/kernel.h>       /* print support */
#include <linux/fs.h>           /* file system support */
#include <linux/io.h>           /* memory mapping support */
#include <linux/interrupt.h>    /* interrupt support */
#include <linux/uaccess.h>      /* sanitize memory accesses */
#include <linux/stddef.h>
#include "sha256.h"

/*
 * global declarations
 */
static bool irq_enable = true;        /* load time parameter to choose irq or polling */
static dev_t dev_number;

static struct sha256_dev sha256_dev;
static struct file_operations sha256_fops = {
  .owner          = THIS_MODULE,
  .llseek         = sha256Llseek,
  .read           = sha256Read,
  .write          = sha256Write,
  .unlocked_ioctl = sha256Ioctl,
  .compat_ioctl   = sha256Ioctl,
  .open           = sha256Open,
  .release        = sha256Release,
  .check_flags    = sha256CheckFlags
};
static struct class *dev_class;

/*
 * parameter handling
 */
module_param(irq_enable, bool, S_IRUGO);
MODULE_PARM_DESC(irq_enable, "Enable interrupt handling (default: true)");

/*
 * file operations
 */

/* File pointer update logic.
 * Compute the final position. There are 3 separate cases:
 *   1. (final position < current position) && not back to start
 *      ERROR: it's not possible to partially revert the hash computation 
 *
 *   2. final position == 0
 *      OK: the next write() call performs a reset
 *
 *   3. final position > current position
 *      OK: if the count of the next write() call is:
 *          0   : there's no additional effect and the write() behaves ignoring the seek 
 *          > 0 : the message is prepended with (final position - current position) bytes at 0
 */
static loff_t sha256Llseek(struct file *filp, loff_t off, int whence) {
  loff_t final_pos;

  if(down_interruptible(&sha256_dev.sem))
    return -ERESTARTSYS;

  switch (whence) {
    case SEEK_SET: final_pos = off; break;
    case SEEK_END: /* END == CUR always */
    case SEEK_CUR: final_pos = sha256_dev.cur_fpos + off; break;
    default: 
                   up(&sha256_dev.sem);

                   PRINTD("llseek() pid = %d: whence not valid", current->pid);
                   return -EINVAL;
  }

  if (!final_pos || final_pos > sha256_dev.cur_fpos) {
    sha256_dev.pending_seek = true;
    sha256_dev.new_fpos = final_pos;
    up(&sha256_dev.sem);

    PRINTD("llseek() pid = %d: final position = %lld", current->pid, final_pos);
    return final_pos;
  }

  if (final_pos == sha256_dev.cur_fpos) {
    up(&sha256_dev.sem);

    PRINTD("llseek() pid = %d: tell current position = %lld", current->pid, final_pos);
    return final_pos;
  }

  up(&sha256_dev.sem);
  PRINTD("llseek() pid = %d: trying to partially revert hash computation", current->pid);
  PRINTD("(cur_fpos = %lld, final_pos = %lld)", sha256_dev.cur_fpos, final_pos);
  return -ESPIPE;
}

static ssize_t sha256Read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {

  if (count > 32) {
    PRINTD("read() pid = %d: requesting %lu bytes, reading 32", current->pid, count);
    count = 32;
  }

  if (down_interruptible(&sha256_dev.sem))
    return -ERESTARTSYS;

  /* check if data is ready upon acquiring the semaphore */
  while (!(SHA256_IOREAD32(sha256_dev.mmap_baseaddr, CSR) & SHA256_MMAP_DONE)) {
    up(&sha256_dev.sem);

    if (filp->f_flags & O_NONBLOCK) {
      PRINTD("read() pid = %d: hash not ready", current->pid);
      return -EAGAIN;
    }

    if (irq_enable) {
      PRINTD("read() pid = %d: interrupt on done: going to sleep", current->pid);

      if (wait_event_interruptible(sha256_dev.rd_q,
            (SHA256_IOREAD32(sha256_dev.mmap_baseaddr, CSR) & SHA256_MMAP_DONE)))
        return -ERESTARTSYS;

    } else {
      PRINTD("read() pid = %d: polling on done", current->pid);
    } 

    /* acquire the semaphore and check again */
    if (down_interruptible(&sha256_dev.sem))
      return -ERESTARTSYS;
  }

  /* hash ready and the semaphore is locked */
  if (irq_enable) {
    if (copy_to_user(buf, sha256_dev.hash_ptr, count)) {
      up(&sha256_dev.sem);
      return -EFAULT;
    }
    PRINTD("read() pid = %d: interrupt, read %lu bytes from kernel buf", current->pid, count);
  } else {
    memcpy_fromio(sha256_dev.hash_ptr, sha256_dev.mmap_baseaddr + offsetof(struct sha256_mmap, H7), 32);
    if (copy_to_user(buf, sha256_dev.hash_ptr, count)) {
      up(&sha256_dev.sem);
      return -EFAULT;
    }
    PRINTD("read() pid = %d: polling, read %lu bytes from io", current->pid, count);
  }

  /* device debug */
  PRINTD("h7       : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, H7));
  PRINTD("h7 kernel: %08X", *((u32*) sha256_dev.hash_ptr));
  PRINTD("h6       : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, H6));
  PRINTD("h6 kernel: %08X", *((u32*) sha256_dev.hash_ptr + 1));
  PRINTD("h5       : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, H5));
  PRINTD("h5 kernel: %08X", *((u32*) sha256_dev.hash_ptr + 2));
  PRINTD("h4       : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, H4));
  PRINTD("h4 kernel: %08X", *((u32*) sha256_dev.hash_ptr + 3));
  PRINTD("h3       : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, H3));
  PRINTD("h3 kernel: %08X", *((u32*) sha256_dev.hash_ptr + 4));
  PRINTD("h2       : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, H2));
  PRINTD("h2 kernel: %08X", *((u32*) sha256_dev.hash_ptr + 5));
  PRINTD("h1       : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, H1));
  PRINTD("h1 kernel: %08X", *((u32*) sha256_dev.hash_ptr + 6));
  PRINTD("h0       : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, H0));
  PRINTD("h0 kernel: %08X", *((u32*) sha256_dev.hash_ptr + 7));

  up(&sha256_dev.sem);
  return count;
}

/* O_NONBLOCK has no effect.
 *
 * msg_concat == false : message concatenation is disabled
 * The operation depends on the byte count:
 *    0   : reset the device (issue new hash) 
 *    > 0 : reset the device (issue new hash) 
 *          transfer the message (issue new write inbetween blocks, synch depends on irq_enable)
 *          request hash (issue last)
 *
 * msg_concat == true : message concatenation is enabled
 * ATTENTION: the message block can't be processed until it's known whether it's the last one or not
 *
 * The operation depends on the byte count:
 *   0    : to mark the end of the concatenated message 
 *          and request the hash (this notifies the block to be processed is the last one)
 *   > 0  : transfer intermediate blocks 
 */
static ssize_t sha256Write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
  ssize_t written_count = 0;

  size_t zero_bytes_count = 0;
  bool rewind = false;

  if (down_interruptible(&sha256_dev.sem))
    return -ERESTARTSYS;

  /* handle pending seek */
  if (sha256_dev.pending_seek) {
    PRINTD("write() pid = %d: detected pending seek", current->pid);
    sha256_dev.pending_seek = false; 

    if (sha256_dev.new_fpos) {
      /* forward seek */
      zero_bytes_count = sha256_dev.new_fpos - sha256_dev.cur_fpos;
      PRINTD("write() pid = %d: forward seek, %lu zero-bytes", current->pid, zero_bytes_count);
    } else
      rewind = true;
  } 

  /* build the message packet */

  /* new hash */ 
  if (rewind || !sha256_dev.msg_concat) {
    SHA256_IOWRITE32(SHA256_MMAP_BASECSR | SHA256_MMAP_NEWHASH, sha256_dev.mmap_baseaddr, CSR);

    sha256_dev.cur_fpos = 0;
    sha256_dev.dev_bytes = 0;
    PRINTD("write() pid = %d: new_hash, cur_fpos = %lld", current->pid, sha256_dev.cur_fpos);
  }

  /* zero bytes */
  if (zero_bytes_count) {
    if (deviceWrite(NULL, zero_bytes_count)) {
      up(&sha256_dev.sem);

      /* handle error by resetting */
      SHA256_IOWRITE32(SHA256_MMAP_BASECSR | SHA256_MMAP_NEWHASH, sha256_dev.mmap_baseaddr, CSR);
      sha256_dev.cur_fpos = 0;
      sha256_dev.dev_bytes = 0;

      PRINTD("write() pid = %d: zero bytes, signal caught waiting on block read, reset", current->pid); 
      return -ERESTARTSYS;
    }

    written_count += zero_bytes_count;
    sha256_dev.cur_fpos += zero_bytes_count;
    PRINTD("write() pid = %d: zero bytes, cur_fpos = %lld", current->pid, sha256_dev.cur_fpos);
  }

  /* user bytes */
  if (count > 0) {
    /* count could exceed the kernel buffer size */
    size_t ccount = count;
    size_t buf_off = 0;

    size_t available_count;

    do {  /* invariant count > 0 */
      PRINTD("write() pid = %d: iteration count = %lu", current->pid, ccount);

      available_count = (ccount > 64) ? 64 : ccount;
      ccount -= available_count;

      /* transfer available_count locally */
      if (copy_from_user(sha256_dev.block_ptr, buf + buf_off, available_count)) {
        up(&sha256_dev.sem);

        PRINTD("write() pid = %d: unable to copy data from user", current->pid);
        return -EFAULT;
      }

      /* update buf pointer */
      buf_off += available_count;
      
      if (deviceWrite(sha256_dev.block_ptr, available_count)) {
        up(&sha256_dev.sem);

        /* handle error by resetting */
        SHA256_IOWRITE32(SHA256_MMAP_BASECSR | SHA256_MMAP_NEWHASH, sha256_dev.mmap_baseaddr, CSR);
        sha256_dev.cur_fpos = 0;
        sha256_dev.dev_bytes = 0;

        PRINTD("write() pid = %d: zero bytes, signal caught waiting on block read, reset", current->pid);
        return -ERESTARTSYS;
      }

    } while (ccount > 0);

    written_count += count;
    sha256_dev.cur_fpos += count;
    PRINTD("write() pid = %d: user bytes, cur_fpos = %lld", current->pid, sha256_dev.cur_fpos);

  }

  /* last */
  if ((zero_bytes_count && !sha256_dev.msg_concat) ||
      (!zero_bytes_count && !rewind && 
       ((!sha256_dev.msg_concat && count > 0) || (sha256_dev.msg_concat && !count) ))) {
    if (deviceLast()) {
      up(&sha256_dev.sem);

      PRINTD("write() pid = %d: unable to write last, no pending block", current->pid);
      return -EIO;
    }

    sha256_dev.cur_fpos = 0;
    sha256_dev.dev_bytes = 0;
    PRINTD("write() pid = %d: last, cur_fpos = %lld", current->pid, sha256_dev.cur_fpos);
  }

  up(&sha256_dev.sem);

  PRINTD("write() pid = %d: written_count = %ld", current->pid, written_count);
  return written_count;
}

/* transfer block words into the device: buf == NULL when for 0 bytes.
 *
 * While doing so: 
 * if there's no more space AND NEW WORDS HAVE TO BE ADDED, then and only then issue a WR
 */
static bool deviceWrite(const char *buf, size_t count) {
  u8 device_buf_size;
  volatile void *device_buf_ptr;

  size_t available_count;

  do {  /* invariant : count > 0 */
    PRINTD("deviceWrite() pid = %d: iteration count = %lu", current->pid, count);

    /* device block could be partially filled */
    device_buf_size = 64 - sha256_dev.dev_bytes;
    device_buf_ptr = sha256_dev.mmap_baseaddr + offsetof(struct sha256_mmap, W0) + sha256_dev.dev_bytes;
    PRINTD("deviceWrite() pid = %d: device_buf_size = %hhu", current->pid, device_buf_size);


    /* if the device block is full it has to be sent, 
     * because there's additional data to be written */
    if (!device_buf_size) { 

      /* device debug */
      PRINTD("w0 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W0));
      PRINTD("w1 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W1));
      PRINTD("w2 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W2));
      PRINTD("w3 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W3));
      PRINTD("w4 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W4));
      PRINTD("w5 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W5));
      PRINTD("w6 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W6));
      PRINTD("w7 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W7));
      PRINTD("w8 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W8));
      PRINTD("w9 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W9));
      PRINTD("w10: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W10));
      PRINTD("w11: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W11));
      PRINTD("w12: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W12));
      PRINTD("w13: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W13));
      PRINTD("w14: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W14));
      PRINTD("w15: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W15));

      /* process block */
      SHA256_IOWRITE32(SHA256_MMAP_BASECSR | SHA256_MMAP_WR | (0xf << 4), sha256_dev.mmap_baseaddr, CSR);

      /* wait for completion on block_read */
      if (irq_enable) {
        PRINTD("deviceWrite() pid = %d: interrupt on block read, going to sleep", current->pid);

        /* wait while holding the semaphore */
        if (wait_event_interruptible(sha256_dev.wr_q, 
              (SHA256_IOREAD32(sha256_dev.mmap_baseaddr, CSR) & SHA256_MMAP_BLOCK_READ))) {
          return true;
        }

        PRINTD("deviceWrite() pid = %d: interrupt on block read, waked up", current->pid);

      } else {
        PRINTD("deviceWrite() pid = %d: polling on block read", current->pid);
        while(!(SHA256_IOREAD32(sha256_dev.mmap_baseaddr, CSR) & SHA256_MMAP_BLOCK_READ));
      }

      /* update count data */
      sha256_dev.dev_bytes = 0;

    } else { /* device_buf_size >= 1 */

      /* fill the current block: min(count, device_buf_size) */
      available_count = (count > device_buf_size) ? device_buf_size : count;  
      count -= available_count;

      if (!buf) {
        /* zero bytes */
        PRINTD("deviceWrite() pid = %d: transferring %lu zero bytes", current->pid, available_count);
        memset_io(device_buf_ptr, 0, available_count);
      } else {
        /* from kernel buffer */
        PRINTD("deviceWrite() pid = %d: transferring %lu bytes from kernel buf", current->pid, available_count);
        memcpy_toio(device_buf_ptr, (void *) buf, available_count);
      }

      /* update count data */
      sha256_dev.dev_bytes += available_count;
      PRINTD("deviceWrite() pid = %d: dev_bytes = %hhu", current->pid, sha256_dev.dev_bytes);
    }

  } while (count > 0);

  return false;
}

/* Writing last is only possible if there's a non-empty pending block in the device.
 * Otherwise, the write fails.
 */
static bool deviceLast(void) {
  u8 last_word, word_byte, field;

  if (!sha256_dev.dev_bytes)
    return true;

  /* update device last word and valid byte */
  last_word = (sha256_dev.dev_bytes-1) / 4;
  word_byte = (sha256_dev.dev_bytes-1) % 4;
  field = (last_word << 2) | ((word_byte + 1) & 0x3);

  PRINTD("deviceLast() pid = %d: last_word = %hhu", current->pid, last_word);
  PRINTD("deviceLast() pid = %d: word_byte = %hhu", current->pid, word_byte);

  /* device debug */
  PRINTD("w0 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W0));
  PRINTD("w1 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W1));
  PRINTD("w2 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W2));
  PRINTD("w3 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W3));
  PRINTD("w4 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W4));
  PRINTD("w5 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W5));
  PRINTD("w6 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W6));
  PRINTD("w7 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W7));
  PRINTD("w8 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W8));
  PRINTD("w9 : %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W9));
  PRINTD("w10: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W10));
  PRINTD("w11: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W11));
  PRINTD("w12: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W12));
  PRINTD("w13: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W13));
  PRINTD("w14: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W14));
  PRINTD("w15: %08X", SHA256_IOREAD32(sha256_dev.mmap_baseaddr, W15));

  SHA256_IOWRITE32(SHA256_MMAP_BASECSR | SHA256_MMAP_WR | SHA256_MMAP_LAST | (field << 2),
      sha256_dev.mmap_baseaddr, CSR); 

  return false; 
}

/* Cryptocore operating modes.
 * By default, write() operations are not concatenated.
 * SHA256_IOC_WCAT allows to change the concatenation option.
 */
static long sha256Ioctl (struct file *filp, unsigned int cmd, unsigned long arg) {
  /* check that no conflict has occurred */
  if (_IOC_TYPE(cmd) != SHA256_IOC_MAGIC) return -ENOTTY;
  if (_IOC_NR(cmd) > SHA256_IOC_MAXNR) return -ENOTTY;

  switch (cmd) {
    case SHA256_IOC_WCAT :  
      sha256_dev.msg_concat = arg; 
      PRINTD("ioctl() pid = %d: msg_concat = %s", current->pid, (sha256_dev.msg_concat) ? "true" : "false");
      break;

    default: /* redundant: already checked */
      PRINTA("unexpected uncaught cmd: %d", cmd);
      return -ENOTTY;
  }

  return 0;
}
static int sha256Open (struct inode *inode, struct file *filp) {
  /* update open counter */
  atomic_inc(&sha256_dev.open_cnt);
  PRINTD("executing open (count = %d)", sha256_dev.open_cnt);

  /* enable cryptocore if actually disabled */
  if (!test_and_set_bit(0, &sha256_dev.skip_core_enable)) {

    SHA256_IOWRITE32(SHA256_MMAP_BASECSR, sha256_dev.mmap_baseaddr, CSR);
  }

  return 0;
}
static int sha256Release (struct inode *inode, struct file *filp) { 
  /* check whether the cryptocore is still used */
  if (atomic_dec_and_test(&sha256_dev.open_cnt)) {
    PRINTD("performing release...");
    SHA256_IOWRITE32(0,sha256_dev.mmap_baseaddr, CSR);
    clear_bit(0, &sha256_dev.skip_core_enable);
  } else {
    PRINTD("skipping release (count = %d)", sha256_dev.open_cnt);
  }
  return 0;
}
static int sha256CheckFlags(int flags) {
  if (flags & (O_APPEND | O_DIRECT | O_NOATIME)) {
    PRINTD("detected invalid flag");
    return -EINVAL;
  }
  return 0;
}


/*
 *  IRQ handling
 */
static irqreturn_t sha256IRQHandler(int irq, void *dev_id) {
  if (SHA256_IOREAD32(sha256_dev.mmap_baseaddr, CSR) & SHA256_MMAP_BLOCK_READ) {
    PRINTD("irq = %d: servicing block_read flag", irq);

    wake_up_interruptible(&sha256_dev.wr_q);

  } else if (SHA256_IOREAD32(sha256_dev.mmap_baseaddr, CSR) & SHA256_MMAP_DONE) {
    memcpy_fromio(sha256_dev.hash_ptr, sha256_dev.mmap_baseaddr + offsetof(struct sha256_mmap, H7), 32);
    PRINTD("irq = %d: servicing done flag: read 32 bytes from io", irq);

    wake_up_interruptible(&sha256_dev.rd_q);

  } else {
    PRINTA("irq = %d: handler found no pending request", irq);
    return IRQ_NONE; 
  }

  return IRQ_HANDLED;
}


/*
 * module dynamic management
 */
static int __init sha256Init(void) {
  int result;
  struct device *dev_model;

  PRINTD("initializing...");

  /* dynamically allocate device number */
  if ((result = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME))) {
    PRINTA("failed device number allocation");
    return result;
  }
  PRINTD("device number allocated: major = %d, minor = %d",
      MAJOR(dev_number), MINOR(dev_number));

  /* register device class */
  dev_class = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(dev_class)) {
    PRINTA("failed class creation");
    result = PTR_ERR(dev_class);
    goto revert_devnumber;
  }
  PRINTD("device class registered");

  /* initialize device representation */

  /* memory mapped segment */
  if (!(request_mem_region(SHA256_BASEADDR, SHA256_MEMSIZE, DEVICE_NAME))) {
    PRINTA("failed requesting memory mapped region");
    result = -ENOMEM;
    goto revert_class;
  }

  if (!(sha256_dev.mmap_baseaddr =  ioremap(SHA256_BASEADDR, SHA256_MEMSIZE))) {
    PRINTA("failed remapping device address segment");
    result = -ENOMEM;
    goto revert_memrq;
  }

  /* concurrency handling */
  atomic_set(&sha256_dev.open_cnt, 0); 
  sha256_dev.skip_core_enable = 0;          
  sema_init(&sha256_dev.sem, 1); 

  /* write behavior */
  sha256_dev.msg_concat = false;
  sha256_dev.dev_bytes = 0; 

  /* seek behavior */
  sha256_dev.pending_seek = false;
  sha256_dev.cur_fpos = 0;
  sha256_dev.new_fpos = 0;

  /* blocking IO support */
  init_waitqueue_head(&sha256_dev.wr_q);
  init_waitqueue_head(&sha256_dev.rd_q);

  /* IO buffers */
  if (!(sha256_dev.hash_ptr = kmalloc(32, GFP_KERNEL))) {
    PRINTA("failed reading buffer allocation");
    result = -ENOMEM;
    goto revert_memrmap;
  }
  if (!(sha256_dev.block_ptr = kmalloc(64, GFP_KERNEL))) {
    PRINTA("failed writing buffer allocation");
    result = -ENOMEM;
    goto revert_hashalloc;
  }

  cdev_init(&sha256_dev.cdev, &sha256_fops);
  sha256_dev.cdev.owner = THIS_MODULE; 
  PRINTD("initialized device representation");

  /* interrupt support */
  if (irq_enable) {
    result = request_irq(SHA256_IRQLINE, sha256IRQHandler, 0, DEVICE_NAME, NULL);
    if (result) {
      PRINTA("failed irq registration");
      goto revert_blockalloc;
    }
    PRINTD("irq registered");
  }

  /* finalize device representation */
  if ((result = cdev_add(&sha256_dev.cdev, dev_number, 1))) {
    PRINTA("failed to notify struct cdev with the kernel");
    goto revert_blockalloc_next;
  }

  /* create device file */
  dev_model = device_create(dev_class, NULL, dev_number, NULL, DEVICE_NAME);
  if (IS_ERR(dev_model)) {
    PRINTA("failed device creation");
    result = PTR_ERR(dev_model);
    goto revert_devadd;
  }

  PRINTD("done initialization...");
  return 0;


revert_devadd          : cdev_del(&sha256_dev.cdev);
revert_blockalloc_next :
                         if (irq_enable)
                           free_irq(SHA256_IRQLINE, &sha256_dev); 

revert_blockalloc      : kfree(sha256_dev.block_ptr);
revert_hashalloc       : kfree(sha256_dev.hash_ptr);
revert_memrmap         : iounmap(sha256_dev.mmap_baseaddr);
revert_memrq           : release_mem_region(SHA256_BASEADDR, SHA256_MEMSIZE);
revert_class           : class_unregister(dev_class);
                         class_destroy(dev_class); 
revert_devnumber       : unregister_chrdev_region(dev_number, 1);

                         return result;
}

static void __exit sha256Exit(void) {
  device_destroy(dev_class, dev_number);
  cdev_del(&sha256_dev.cdev);

  if (irq_enable) {
    free_irq(SHA256_IRQLINE, &sha256_dev); 
  }

  kfree(sha256_dev.block_ptr);
  kfree(sha256_dev.hash_ptr);
  iounmap(sha256_dev.mmap_baseaddr);
  release_mem_region(SHA256_BASEADDR, SHA256_MEMSIZE);
  class_unregister(dev_class);
  class_destroy(dev_class); 
  unregister_chrdev_region(dev_number, 1);
}

module_init(sha256Init);
module_exit(sha256Exit);

/* 
 * descriptive definitions 
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("F. Scatozza, I. Delpero, L. Cerruti, C. Capobianchi, S. Alipour, A. Rehman, S. Perera");
MODULE_DESCRIPTION("Driver for the gv_sha256 cryptocore implemented in the pynq-z2 board.");
MODULE_VERSION("0.1");
