#include "ff.h"
#include "contiki.h"
#include "cfs/cfs.h"

#define FILE_TABLE_SIZE 5

static uint8_t inited = 0;

static FATFS fs;
static FIL file_table[FILE_TABLE_SIZE];

static int cfs_fatfs_init()
{
  uint8_t i;
  /* mark files as empty */
  for (i = 0; i < FILE_TABLE_SIZE; i++)
    file_table[i].fs = NULL;
  
  /* mount fatfs */
  if (!f_mount(0, &fs)) {
    inited = 1;
    return 0;
  } else
    return -1;
}


int cfs_open(const char *name, int flags)
{
  uint8_t i;

  if (!inited)
    cfs_fatfs_init();

  for (i = 0; i < FILE_TABLE_SIZE; i++) {
    if (file_table[i].fs == NULL) {
      /* only read and write flag */
      if (!f_open(&file_table[i], name, (flags & 0x3) | FA_OPEN_ALWAYS)) {
        if (flags & 0x4) {
          f_lseek(&file_table[i], f_size(&file_table[i]));
        }
        return i;
      } else
        return -1;
    }
  }
  return -1;
}

void cfs_close(int fd)
{
  if (inited && file_table[fd].fs) {
    f_close(&file_table[fd]);
    file_table[fd].fs = NULL;
  }
}

int cfs_read(int fd, void *buf, unsigned int len)
{
  if (inited && file_table[fd].fs) {
    f_read(&file_table[fd], buf, len, &len);
    return len;
  }
  return 0;
}

int cfs_write(int fd, const void *buf, unsigned int len)
{
  if (inited && file_table[fd].fs) {
    f_write(&file_table[fd], buf, len, &len);
    return len;
  }
  return 0; 
}

int cfs_seek(int fd, cfs_offset_t offset, int whence)
{
  uint32_t new_offset;
  if (inited && file_table[fd].fs) {
    switch (whence) {
      case CFS_SEEK_CUR:
        new_offset = f_tell(&file_table[fd]) + offset;
        break;
      case CFS_SEEK_END:
        new_offset = f_size(&file_table[fd]) + offset;
        break;
      case CFS_SEEK_SET:
      default:
        new_offset = offset;
        break;
    }
    f_lseek(&file_table[fd], new_offset);
    return new_offset;
  }
  return 0;
}


/* dir functions not implemented */
int cfs_opendir (struct cfs_dir *dirp, const char *name)
{
  return -1;
}
int cfs_readdir (struct cfs_dir *dirp, struct cfs_dirent *dirent)
{
  return -1;
}
void cfs_closedir (struct cfs_dir *dirp)
{
}
