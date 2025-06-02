#ifndef SPECIALFILEVFS_H
#define SPECIALFILEVFS_H
#include <stdint.h>
#include "../vfs.h"
#include "../tmpfs.h"
#define MBOX_REQUEST 0
#define MBOX_CH_PROP 8
#define MBOX_TAG_LAST 0
void framebuffer_vfs_init(void);
int framebuffer_init(unsigned int req_width, unsigned int req_height, unsigned int req_depth);
static int fb_dev_write(struct file* f, const void* buf, size_t len);
static int fb_dev_read(struct file* f, void* buf, size_t len);
static long fb_dev_lseek64(struct file* file, long offset, int whence);
static int fb_dev_ioctl(struct file* file, unsigned long request, void* argp);
extern struct file_operations fb_dev_file_ops;
#endif