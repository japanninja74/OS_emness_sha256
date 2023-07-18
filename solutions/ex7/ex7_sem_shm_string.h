/* ex7_sem_shm_string.h

    Licensed under GNU General Public License v2 or later.
*/

#define EMAIL_LENGTH 64+1 //+1 for trailing '\0'
#define HASH_LENGTH 64+1
#define MSG_SIZE 64+1
const char sem_path[]="/tmp";
const char shm_path[]="/sys";
union semun {                   /* Used in calls to semctl() */
    int                 val;
    struct semid_ds *   buf;
    unsigned short *    array;
#if defined(__linux__)
    struct seminfo *    __buf;
#endif
};
