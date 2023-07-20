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

#include <linux/kernel.h>       /* misc support */
#include <linux/init.h>         /* initialization & cleanup */
#include <linux/module.h>       /* core header for LKMs */
#include <linux/sched.h>        /* kernel api */
#include <linux/slab.h>         /* dynamic memory allocation */
#include <linux/io.h>           /* memory mapping */
#include <linux/uaccess.h>      /* safe memory accesses */
#include <linux/interrupt.h>    /* interrupt */
#include <linux/iopoll.h>       /* dedicated polling strategy */

#include <linux/moduleparam.h>  /* support for load-time parameters */
#include <linux/stddef.h>       /* utility macros */
#include <linux/fs.h>           /* file system support */

#include <linux/of_address.h>   /* platform bus support */
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include "sha256.h"
/*
 * global declarations
 */
static bool irq_enable = false;        /* load time: choose irq or polling synchronization */

/* device variables */
static struct class *dev_class;
static dev_t dev_number;
static struct sha256_dev sha256_dev;

/* bind the driver to the device */
#ifdef CONFIG_OF
static struct of_device_id sha256_of_match[] = {
  { .compatible  = "xlnx,gv-sha256-1.0", },
  { /* end of list */ },
};
MODULE_DEVICE_TABLE(of, sha256_of_match);
#else
# define sha256_of_match
#endif

/* driver representation */
static int sha256Probe(struct platform_device *pdev);  /* device found while probing: driver startup */
static int sha256Remove(struct platform_device *pdev); /* device removed: driver unloading */

static struct platform_driver sha256_driver = {
  .driver = {
    .name = DRIVER_NAME,
    .owner = THIS_MODULE,
    .of_match_table     = sha256_of_match,
  },
  .probe                = sha256Probe,
  .remove               = sha256Remove,
};

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

/*
 * parameter handling
 */
module_param(irq_enable, bool, S_IRUGO);
MODULE_PARM_DESC(irq_enable, "Enable interrupt handling (default: false)");

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

                   PR_DEVEL("llseek() pid = %d: whence not valid", current->pid);
                   return -EINVAL;
  }

  if (!final_pos || final_pos > sha256_dev.cur_fpos) {
    sha256_dev.pending_seek = true;
    sha256_dev.new_fpos = final_pos;
    up(&sha256_dev.sem);

    PR_DEVEL("llseek() pid = %d: final position = %lld", current->pid, final_pos);
    return final_pos;
  }
  
  up(&sha256_dev.sem);

  if (final_pos == sha256_dev.cur_fpos) {
    PR_DEVEL("llseek() pid = %d: tell current position = %lld", current->pid, final_pos);
    return final_pos;
  }

  PR_ALERT("llseek() pid = %d: trying to partially revert hash computation", current->pid);
  PR_ALERT("(cur_fpos = %lld, final_pos = %lld)", sha256_dev.cur_fpos, final_pos);
  return -ESPIPE;
}

static ssize_t sha256Read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
  u32 csr;

  if (count > 32) {
    PR_ALERT("read() pid = %d: requesting %lu bytes, reading 32", current->pid, count);
    count = 32;
  }

  if (down_interruptible(&sha256_dev.sem))
    return -ERESTARTSYS;

  /* check if data is ready upon acquiring the semaphore */
  while (!(SHA256_IOREAD32(sha256_dev.base_addr, CSR) & SHA256_MMAP_DONE)) {
    up(&sha256_dev.sem);

    if (filp->f_flags & O_NONBLOCK) {
      PR_DEVEL("read() pid = %d: hash not ready", current->pid);
      return -EAGAIN;
    }

    if (irq_enable) {
      PR_DEVEL("read() pid = %d: interrupt on done: going to sleep", current->pid);

      if (wait_event_interruptible(sha256_dev.rd_q,
            (SHA256_IOREAD32(sha256_dev.base_addr, CSR) & SHA256_MMAP_DONE)))
        return -ERESTARTSYS;

    } else {
      PR_DEVEL("read() pid = %d: readx_poll_timeout() on done", current->pid);
      readx_poll_timeout(ioread32, sha256_dev.base_addr + offsetof(struct sha256_mmap, CSR), 
          csr, csr & SHA256_MMAP_DONE, POLLING_SLEEP_US, 0);
      PR_DEVEL("read() pid = %d: readx_poll_timeout(): waked up", current->pid);
    }

    /* acquire the semaphore and check again */
    if (down_interruptible(&sha256_dev.sem))
      return -ERESTARTSYS;
  }

  /* hash ready and the semaphore is locked */
  if (!irq_enable) {
    memcpy_fromio(sha256_dev.hash_ptr, sha256_dev.base_addr + offsetof(struct sha256_mmap, H7), 32);
    PR_DEVEL("read() pid = %d: polling, read %lu bytes from io", current->pid, count);
  }

  if (copy_to_user(buf, sha256_dev.hash_ptr, count)) {
    up(&sha256_dev.sem);
    return -EFAULT;
  }

  /* device debug */
  PR_DEVEL("h7 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, H7));
  PR_DEVEL("h6 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, H6));
  PR_DEVEL("h5 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, H5));
  PR_DEVEL("h4 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, H4));
  PR_DEVEL("h3 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, H3));
  PR_DEVEL("h2 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, H2));
  PR_DEVEL("h1 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, H1));
  PR_DEVEL("h0 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, H0));

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
    PR_DEVEL("write() pid = %d: detected pending seek", current->pid);
    sha256_dev.pending_seek = false; 

    if (sha256_dev.new_fpos) {
      /* forward seek */
      zero_bytes_count = sha256_dev.new_fpos - sha256_dev.cur_fpos;
      PR_DEVEL("write() pid = %d: forward seek, %lu zero-bytes", current->pid, zero_bytes_count);
    } else
      rewind = true;
  } 

  /* build the message packet */

  /* new hash */ 
  if (rewind || !sha256_dev.msg_concat) {
    SHA256_IOWRITE32(SHA256_MMAP_BASECSR | SHA256_MMAP_NEWHASH, sha256_dev.base_addr, CSR);

    sha256_dev.cur_fpos = 0;
    sha256_dev.dev_bytes = 0;
    PR_DEVEL("write() pid = %d: new_hash, cur_fpos = %lld", current->pid, sha256_dev.cur_fpos);
  }

  /* zero bytes */
  if (zero_bytes_count) {
    if (deviceWrite(NULL, zero_bytes_count)) {
      up(&sha256_dev.sem);

      PR_DEVEL("write() pid = %d: zero bytes, signal caught waiting on block read, reset", current->pid); 
      written_count = -ERESTARTSYS;
      goto handle_by_resetting;
    }

    written_count += zero_bytes_count;
    sha256_dev.cur_fpos += zero_bytes_count;
    PR_DEVEL("write() pid = %d: zero bytes, cur_fpos = %lld", current->pid, sha256_dev.cur_fpos);
  }

  /* user bytes */
  if (count > 0) {

    /* count could exceed the kernel buffer size */
    size_t ccount = count;
    size_t buf_off = 0;

    size_t available_count;

    do {  /* invariant count > 0 */
      PR_DEVEL("write() pid = %d: iteration count = %lu", current->pid, ccount);

      available_count = (ccount > 64) ? 64 : ccount; /* available_count doesn't exceed the buffer */
      ccount -= available_count;

      /* transfer available_count locally */
      if (copy_from_user(sha256_dev.block_ptr, buf + buf_off, available_count)) {
        up(&sha256_dev.sem);

        PR_ALERT("write() pid = %d: unable to copy data from user", current->pid);
        written_count = -EFAULT;
        goto handle_by_resetting;
      }

      /* update buf pointer */
      buf_off += available_count;
      
      if (deviceWrite(sha256_dev.block_ptr, available_count)) {
        up(&sha256_dev.sem);

        PR_DEVEL("write() pid = %d: zero bytes, signal caught waiting on block read, reset", current->pid);
        written_count = -ERESTARTSYS;
        goto handle_by_resetting;
      }

    } while (ccount > 0);

    written_count += count;
    sha256_dev.cur_fpos += count;
    PR_DEVEL("write() pid = %d: user bytes, cur_fpos = %lld", current->pid, sha256_dev.cur_fpos);

  }

  /* last */
  if ((zero_bytes_count && !sha256_dev.msg_concat) ||
      (!zero_bytes_count && !rewind && 
       ((!sha256_dev.msg_concat && count > 0) || (sha256_dev.msg_concat && !count) ))) {
    if (deviceLast()) {
      up(&sha256_dev.sem);

      PR_ALERT("write() pid = %d: unable to write last, no pending block", current->pid);
      return -EIO;
    }

    sha256_dev.cur_fpos = 0;
    sha256_dev.dev_bytes = 0;
    PR_DEVEL("write() pid = %d: last, cur_fpos = %lld", current->pid, sha256_dev.cur_fpos);
  }

  up(&sha256_dev.sem);

  PR_DEVEL("write() pid = %d: written_count = %ld", current->pid, written_count);
  return written_count;

handle_by_resetting: 
  SHA256_IOWRITE32(SHA256_MMAP_BASECSR | SHA256_MMAP_NEWHASH, sha256_dev.base_addr, CSR);
  sha256_dev.cur_fpos = 0;
  sha256_dev.dev_bytes = 0;
  
  return written_count;
}

/* transfer block words into the device: buf == NULL when for 0 bytes.
 *
 * While doing so: 
 * if there's no more space AND NEW WORDS HAVE TO BE ADDED, then and only then issue a WR
 */
static bool deviceWrite(const char *buf, size_t count) {
  u32 csr;
  u8 device_buf_size;
  volatile void *device_buf_ptr;

  size_t available_count;

  do {  /* invariant : count > 0 */
    PR_DEVEL("deviceWrite() pid = %d: iteration count = %lu", current->pid, count);

    /* device block could be partially filled */
    device_buf_size = 64 - sha256_dev.dev_bytes;
    device_buf_ptr = sha256_dev.base_addr + offsetof(struct sha256_mmap, W0) + sha256_dev.dev_bytes;
    PR_DEVEL("deviceWrite() pid = %d: device_buf_size = %hhu", current->pid, device_buf_size);


    /* if the device block is full it has to be sent, 
     * because there's additional data to be written */
    if (!device_buf_size) { 

      /* device debug */
      PR_DEVEL("w0 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W0));
      PR_DEVEL("w1 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W1));
      PR_DEVEL("w2 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W2));
      PR_DEVEL("w3 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W3));
      PR_DEVEL("w4 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W4));
      PR_DEVEL("w5 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W5));
      PR_DEVEL("w6 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W6));
      PR_DEVEL("w7 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W7));
      PR_DEVEL("w8 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W8));
      PR_DEVEL("w9 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W9));
      PR_DEVEL("w10: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W10));
      PR_DEVEL("w11: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W11));
      PR_DEVEL("w12: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W12));
      PR_DEVEL("w13: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W13));
      PR_DEVEL("w14: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W14));
      PR_DEVEL("w15: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W15));

      /* process block */
      SHA256_IOWRITE32(SHA256_MMAP_BASECSR | SHA256_MMAP_WR | (0xf << 4), sha256_dev.base_addr, CSR);

      /* wait for completion on block_read */
      if (irq_enable) {
        PR_DEVEL("deviceWrite() pid = %d: interrupt on block read, going to sleep", current->pid);

        /* wait while holding the semaphore */
        if (wait_event_interruptible(sha256_dev.wr_q, 
              (SHA256_IOREAD32(sha256_dev.base_addr, CSR) & SHA256_MMAP_BLOCK_READ))) {
          return true;
        }

        PR_DEVEL("deviceWrite() pid = %d: interrupt on block read, waked up", current->pid);

      } else {
        PR_DEVEL("deviceWrite() pid = %d: readx_poll_timeout() on block read", current->pid);
        readx_poll_timeout(ioread32, sha256_dev.base_addr + offsetof(struct sha256_mmap, CSR), 
            csr, csr & SHA256_MMAP_BLOCK_READ, POLLING_SLEEP_US, 0);
        PR_DEVEL("deviceWrite() pid = %d: readx_poll_timeout(): waked up", current->pid);
      }

      /* update count data */
      sha256_dev.dev_bytes = 0;

    } else { /* device_buf_size >= 1 */

      /* fill the current block: min(count, device_buf_size) */
      available_count = (count > device_buf_size) ? device_buf_size : count;  
      count -= available_count;

      if (!buf) {
        /* zero bytes */
        PR_DEVEL("deviceWrite() pid = %d: transferring %lu zero bytes", current->pid, available_count);
        memset_io(device_buf_ptr, 0, available_count);
      } else {
        /* from kernel buffer */
        PR_DEVEL("deviceWrite() pid = %d: transferring %lu bytes from kernel buf", current->pid, available_count);
        memcpy_toio(device_buf_ptr, (void *) buf, available_count);
      }

      /* update count data */
      sha256_dev.dev_bytes += available_count;
      PR_DEVEL("deviceWrite() pid = %d: dev_bytes = %hhu", current->pid, sha256_dev.dev_bytes);
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

  PR_DEVEL("deviceLast() pid = %d: last_word = %hhu", current->pid, last_word);
  PR_DEVEL("deviceLast() pid = %d: word_byte = %hhu", current->pid, word_byte);

  /* device debug */
  PR_DEVEL("w0 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W0));
  PR_DEVEL("w1 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W1));
  PR_DEVEL("w2 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W2));
  PR_DEVEL("w3 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W3));
  PR_DEVEL("w4 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W4));
  PR_DEVEL("w5 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W5));
  PR_DEVEL("w6 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W6));
  PR_DEVEL("w7 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W7));
  PR_DEVEL("w8 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W8));
  PR_DEVEL("w9 : %08X", SHA256_IOREAD32(sha256_dev.base_addr, W9));
  PR_DEVEL("w10: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W10));
  PR_DEVEL("w11: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W11));
  PR_DEVEL("w12: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W12));
  PR_DEVEL("w13: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W13));
  PR_DEVEL("w14: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W14));
  PR_DEVEL("w15: %08X", SHA256_IOREAD32(sha256_dev.base_addr, W15));

  SHA256_IOWRITE32(SHA256_MMAP_BASECSR | SHA256_MMAP_WR | SHA256_MMAP_LAST | (field << 2),
      sha256_dev.base_addr, CSR);

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
      PR_DEVEL("ioctl() pid = %d: msg_concat = %s", current->pid, (sha256_dev.msg_concat) ? "true" : "false");
      break;

    default: /* redundant: already checked */
      PR_ALERT("unexpected uncaught cmd: %d", cmd);
      return -ENOTTY;
  }

  return 0;
}

static int sha256Open (struct inode *inode, struct file *filp) {
  /* update open counter */
  atomic_inc(&sha256_dev.open_cnt);
  PR_DEVEL("executing open (count = %d)", atomic_read(&sha256_dev.open_cnt));

  /* enable cryptocore if actually disabled */
  if (!test_and_set_bit(0, &sha256_dev.skip_core_enable))
    SHA256_IOWRITE32(SHA256_MMAP_BASECSR, sha256_dev.base_addr, CSR);

  return 0;
}
static int sha256Release (struct inode *inode, struct file *filp) { 
  /* check whether the cryptocore is still used */
  if (atomic_dec_and_test(&sha256_dev.open_cnt)) {
    PR_DEVEL("performing release...");
    SHA256_IOWRITE32(0,sha256_dev.base_addr, CSR);
    clear_bit(0, &sha256_dev.skip_core_enable);
  } else {
    PR_DEVEL("skipping release (count = %d)", atomic_read(&sha256_dev.open_cnt));
  }
  return 0;
}
static int sha256CheckFlags(int flags) {
  if (flags & (O_APPEND | O_DIRECT | O_NOATIME)) {
    PR_DEVEL("detected invalid flag");
    return -EINVAL;
  }
  return 0;
}

/*
 *  IRQ handling
 */
static irqreturn_t sha256IRQHandler(int irq, void *dev_id) {
  if (SHA256_IOREAD32(sha256_dev.base_addr, CSR) & SHA256_MMAP_BLOCK_READ) {
    PR_DEVEL("irq = %d: servicing block_read flag", irq);

    wake_up_interruptible(&sha256_dev.wr_q);

  } else if (SHA256_IOREAD32(sha256_dev.base_addr, CSR) & SHA256_MMAP_DONE) {
    memcpy_fromio(sha256_dev.hash_ptr, sha256_dev.base_addr + offsetof(struct sha256_mmap, H7), 32);
    PR_DEVEL("irq = %d: servicing done flag: read 32 bytes from io", irq);

    wake_up_interruptible(&sha256_dev.rd_q);

  } else {
    PR_ALERT("irq = %d: handler found no pending request", irq);
    return IRQ_NONE; 
  }

  return IRQ_HANDLED;
}


/*
 * module dynamic management
 */
static int sha256Probe(struct platform_device *pdev) {
  struct resource *r_mem; /* IO mem resources */
  struct device *dev = &pdev->dev;
  
  int rc = 0;

  dev_info(dev, "device tree probing\n");

  /* dynamically allocate device number */
  if ((rc = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME)) < 0) {
    dev_err(dev, "failed device number allocation\n");
    return rc;
  }
  dev_info(dev, "device number allocated: major = %d, minor = %d\n", MAJOR(dev_number), MINOR(dev_number));

  /* register device class */
  dev_class = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(dev_class)) {
    dev_err(dev, "failed class creation\n");
    rc = PTR_ERR(dev_class);
    goto revert_chrdev;
  }
  dev_info(dev, "device class registered\n");

  /* initialize device representation */

  cdev_init(&sha256_dev.cdev, &sha256_fops);
  sha256_dev.cdev.owner = THIS_MODULE; 

  /* memory mapped segment */
  r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!r_mem) {
    dev_err(dev, "invalid address\n");
    rc = -ENODEV;
    goto revert_class;
  }
  dev_info(dev, "found memory mapped segment\n");
  
  sha256_dev.mem_start = r_mem->start;
  sha256_dev.mem_end = r_mem->end;

  if (!request_mem_region(sha256_dev.mem_start,
        sha256_dev.mem_end - sha256_dev.mem_start + 1, DRIVER_NAME)) {
    dev_err(dev, "couldn't lock memory region at %p\n", (void *)sha256_dev.mem_start);
    rc = -EBUSY;
    goto revert_class;
  }

  sha256_dev.base_addr = 
    ioremap(sha256_dev.mem_start,sha256_dev.mem_end - sha256_dev.mem_start + 1);
  if (!sha256_dev.base_addr) {
    dev_err(dev, "could not allocate iomem\n");
    rc = -EIO;
    goto revert_memrq;
  }
  dev_info(dev, "%p mapped to %p\n", 
      (void*)sha256_dev.mem_start, (void*)sha256_dev.base_addr);

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
    dev_err(dev, "failed reading buffer allocation\n");
    rc = -ENOMEM;
    goto revert_memrmap;
  }
  if (!(sha256_dev.block_ptr = kmalloc(64, GFP_KERNEL))) {
    dev_err(dev, "failed writing buffer allocation\n");
    rc = -ENOMEM;
    goto revert_hashalloc;
  }

  /* Get IRQ for the device */
  if (irq_enable) {
      
    sha256_dev.irq = platform_get_irq(pdev, 0);
    if (sha256_dev.irq < 0) {
      dev_err(dev, "unable to get irq number\n");
      rc = sha256_dev.irq; 
      goto revert_blockalloc;
    }
    dev_info(dev, "irq = %d\n", sha256_dev.irq);

    rc = request_irq(sha256_dev.irq, sha256IRQHandler, 
        IRQF_TRIGGER_RISING, DRIVER_NAME, &sha256_dev);
    if (rc < 0) {
      dev_err(dev, "could not allocate interrupt\n");
      goto revert_blockalloc;
    }
    dev_info(dev, "irq registered\n");

  } /* irq_enable */


  /* finalize device representation */
  if ((rc = cdev_add(&sha256_dev.cdev, dev_number, 1)) < 0) {
    dev_err(dev, "failed to notify struct cdev to the kernel\n");
    goto revert_irq_blockallock;
  }
  dev_info(dev, "done initialization\n");

  return 0;

revert_irq_blockallock : 

  if (irq_enable) {
    free_irq(sha256_dev.irq, &sha256_dev); 
  }

revert_blockalloc      : kfree(sha256_dev.block_ptr);
revert_hashalloc       : kfree(sha256_dev.hash_ptr);
revert_memrmap         : iounmap(sha256_dev.base_addr);
revert_memrq           : release_mem_region(sha256_dev.mem_start, sha256_dev.mem_end - sha256_dev.mem_start + 1);
revert_class           : class_unregister(dev_class);
                         class_destroy(dev_class); 
revert_chrdev          : unregister_chrdev_region(dev_number, 1);

                         return rc;
}

static int __init sha256Init(void) {
  struct device *dev;
  int rc;

  PR_INFO("sha256Init(): irq_enable = %s", (irq_enable ? "true" : "false"));

  /* register the driver */ 
  if ((rc = platform_driver_register(&sha256_driver)) < 0) {
    PR_ALERT("failed platform driver registration");

    return rc;
  }
  PR_INFO("platform driver registered");

  /* create device file */
  dev = device_create(dev_class, NULL, dev_number, NULL, DEVICE_NAME);
  if (IS_ERR(dev)) {
    PR_ALERT("failed device file creation");

    platform_driver_unregister(&sha256_driver);
    return PTR_ERR(dev);
  }
  PR_INFO("device file created");

  return 0;

}

static int sha256Remove(struct platform_device *pdev) {
  PR_DEVEL("sha256Remove()");

  cdev_del(&sha256_dev.cdev);

  if (irq_enable)
    free_irq(sha256_dev.irq, &sha256_dev); 

  kfree(sha256_dev.block_ptr);
  kfree(sha256_dev.hash_ptr);
  iounmap(sha256_dev.base_addr);
  release_mem_region(sha256_dev.mem_start, sha256_dev.mem_end - sha256_dev.mem_start + 1);
  class_unregister(dev_class);
  class_destroy(dev_class); 
  unregister_chrdev_region(dev_number, 1);

  return 0;
}

static void __exit sha256Exit(void) {
  PR_INFO("sha256Exit()");

  device_destroy(dev_class, dev_number);
  platform_driver_unregister(&sha256_driver);
}

module_init(sha256Init);
module_exit(sha256Exit);

/*
 * descriptive definitions
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("F. Scatozza, I. Delpero, L. Cerruti, C. Capobianchi, S. Alipour, A. Rehman, S. Perera");
MODULE_DESCRIPTION("Driver for the gv_sha256 cryptocore implemented in the pynq-z2 board.");
MODULE_VERSION("0.1.1");

