#include <linux/ioctl.h>

#define IOC_MAGIC 'k'
#define IOCTL_PATTERN _IOW(IOC_MAGIC, 0, unsigned long) 

