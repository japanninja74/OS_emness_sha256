/**
 * File              : self_test.c
 *                     
 * Authors           : Fabio Scatozza      <s315216@studenti.polito.it>
 *                     Isacco Delpero      <s314713@studenti.polito.it>
 *                     Leonardo Cerruti    <s317664@studenti.polito.it>
 *                     Claudio Capobianchi <s319187@studenti.polito.it>
 *                     Saman Alipour       <s307980@studenti.polito.it>
 *                     Abdul Rehman        <s315198@studenti.polito.it>
 *                     Stephano Perera     <s313080@studenti.polito.it>
 *
 * Date              : 10.07.2023
 * Last Modified Date: 19.07.2023
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SHA256_IOC_MAGIC  'S'
#define SHA256_IOC_WCAT   _IOW(SHA256_IOC_MAGIC, 1, int)

/* test data */

static const char str[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopqopqrpqrsqrstrstus"
                          "tuvtuvwuvwxvwxywxyz";

static const unsigned hash_str[] = {
  0x2d7cfdd1,
  0x4a49f8c9,
  0xaecabdda,
  0xf3c4560b,
  0x19b7fb21,
  0xc7639199,
  0x3688b034,
  0x9d30c975 };

static const unsigned hash_1000000zero[] = {
  0xd29751f2,
  0x649b32ff,
  0x572b5e0a,
  0x9f541ea6,
  0x60a50f94,
  0xff0beedf,
  0xb0b692b9,
  0x24cc8025};

static const unsigned hash_10zero_abc[] = {
  0x613f318b,
  0xec31b432,
  0x76e3355f,
  0x0e91cd28,
  0xe0547b03,
  0x0a3daf0d,
  0xf9637f54,
  0xe5ab6f01};

static const unsigned hash_abc_10zero_abc_10zero[] = {
  0xbaafff5f,
  0xd15d4354,
  0x3346ee97,
  0x5748fb44,
  0x8f721729,
  0x2cc83464,
  0x8ee986be,
  0xb851adce};

void printHash(const unsigned hash[8]);
bool validateHash(const unsigned hash[8], const unsigned hash_xpected[8]);

void readFancy(int fd, const unsigned hash_xpected[8]);

int main() {
  int fd;
  long pos, count;

  printf("################################\n");
  printf("# Beginning self test...\n");
  printf("################################\n");

  /* 
   * testing open 
   */
  if((fd = open("/dev/sha256", O_RDWR)) < 0) {
    perror("open()");
    exit(EXIT_FAILURE);
  }

  printf("open() done: blocking, read-write access\n");

  /* 
   * testing write without concatenation 
   */
  printf("\n################################\n");
  printf("Testing write without concatenation...\n");
  printf("msg: %s\n", str);

  if ((count = write(fd, str, 92)) < 0)
    perror("write()");
  else if (count != 92)
   fprintf(stderr, "write(): failed writing 92 bytes\n"); 

  readFancy(fd, hash_str);
  
  /* 
   * testing zero-message 
   */
  printf("\n################################\n");
  printf("Testing write without concatenation + forward seek...\n");
  printf("1) msg: 1_000_000 zero-bytes\n");

  if ((pos = lseek(fd, 0, SEEK_CUR)) < 0)
    perror("lseek()");
  else
    printf("lseek(): cur_fpos = %ld\n", pos);

  if ((pos = lseek(fd, 1000000, SEEK_END)) < 0)
    perror("lseek()");
  else
    printf("lseek(): new_fpos = %ld\n", pos);

  if ((count = write(fd, NULL, 0)) < 0)
    perror("write()");
  else
    printf("write(): written %ld byte(s)\n", count);

  readFancy(fd, hash_1000000zero);


  puts("");
  printf("2) msg: 10 zero-bytes, \"abc\"\n");

  if ((pos = lseek(fd, 0, SEEK_CUR)) < 0)
    perror("lseek()");
  else
    printf("lseek(): cur_fpos = %ld\n", pos);

  if ((pos = lseek(fd, 10, SEEK_END)) < 0)
    perror("lseek()");
  else
    printf("lseek(): new_fpos = %ld (zero-bytes)\n", pos);

  if ((count = write(fd, str, 3)) < 0)
    perror("write()");
  else
    printf("write(): written %ld byte(s) (zero-bytes + \"abc\")\n", count);

  readFancy(fd, hash_10zero_abc);

  /*
   * testing concatenations
   */
  printf("\n################################\n");
  printf("Testing concatenations...\n");
  
  printf("Reset by writing 0 without concatenations\n");
  if ((count = write(fd, NULL, 0)) < 0)
    perror("write()");
  else
    printf("write(): written %ld byte(s)\n", count);

  printf("Enabling concatenations...\n");
  if(ioctl(fd, SHA256_IOC_WCAT, 1) < 0) {
    perror("ioctl()");
    close(fd);
    exit(EXIT_FAILURE);
  }

  puts("");
  printf("enqueue: %s\n", str);
  if ((count = write(fd, str, 92)) < 0)
    perror("write()");
  else if (count != 92)
   fprintf(stderr, "write(): failed writing 92 bytes\n"); 

  if ((pos = lseek(fd, 0, SEEK_CUR)) < 0)
    perror("lseek()");
  else
    printf("lseek(): cur_fpos = %ld\n", pos);
 
  puts("");
  printf("Reset by rewind\n");
  if ((pos = lseek(fd, 0, SEEK_SET)) < 0)
    perror("lseek()");
  else
    printf("lseek(): cur_fpos = %ld\n", pos);

  /* actual concatenation: abc, 10 zero-bytes, abc, 10 zero-bytes*/
  puts("");
  printf("enqueue: \"abc\"\n");
  if ((count = write(fd, str, 3)) < 0)
    perror("write()");
  else
    printf("write(): written %ld byte(s)\n", count);


  puts("");
  printf("enqueue: 10 zero-bytes, \"abc\"\n");
  if ((pos = lseek(fd, 0, SEEK_CUR)) < 0)
    perror("lseek()");
  else
    printf("lseek(): cur_fpos = %ld\n", pos);

  if ((pos = lseek(fd, 10, SEEK_END)) < 0)
    perror("lseek()");
  else
    printf("lseek(): new_fpos = %ld (zero-bytes)\n", pos);

  if ((count = write(fd, str, 3)) < 0)
    perror("write()");
  else
    printf("write(): written %ld byte(s) (zero-bytes + \"abc\")\n", count);


  puts("");
  printf("enqueue: 10 zero-bytes\n");
  if ((pos = lseek(fd, 0, SEEK_CUR)) < 0)
    perror("lseek()");
  else
    printf("lseek(): cur_fpos = %ld\n", pos);

  if ((pos = lseek(fd, 10, SEEK_END)) < 0)
    perror("lseek()");
  else
    printf("lseek(): new_fpos = %ld (zero-bytes)\n", pos);
  
  if ((count = write(fd, NULL, 0)) < 0)
    perror("write()");
  else
    printf("write(): written %ld byte(s) (zero-bytes)\n", count);

  
  puts("");
  printf("terminate concatenation...\n");
  if ((count = write(fd, NULL, 0)) < 0)
    perror("write()");
  else
    printf("write(): written %ld byte(s) (issue last)\n", count);

  readFancy(fd, hash_abc_10zero_abc_10zero);
  
  printf("\n################################\n");
  
  printf("Disabling concatenations...\n");
  if(ioctl(fd, SHA256_IOC_WCAT, 0) < 0) {
    perror("ioctl()");
    close(fd);
    exit(EXIT_FAILURE);
  }

  close(fd);
  exit(EXIT_SUCCESS);
}


void printHash(const unsigned hash[8]) {
  printf("hash: %08X", hash[0]);

  for (int i = 1; i < 8; i++)
   printf("_%08X", hash[i]); 

  puts(""); 
}

bool validateHash(const unsigned hash[8], const unsigned hash_xpected[8]) {
  bool result = true;

  for (int i = 0; i<8; i++)
   result &= (hash[i] == hash_xpected[i]);

  return result;
}

void readFancy(int fd, const unsigned hash_xpected[8]) {
  long count;
  unsigned hash_read[8];

  if ((count = read(fd, hash_read, 32)) < 0)
    perror("read()");
  else if (count != 32)
    fprintf(stderr, "read(): failed reading 32 bytes\n"); 
  else {

    printHash(hash_read);
    
    if (!validateHash(hash_read, hash_xpected))
      fprintf(stderr, "hash mismatch!\n"); 
    else
      printf("match!\n");

  }
}
