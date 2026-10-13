#ifndef EDU_IOCTL_H
#define EDU_IOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

struct edu_info {
    __u32 abi_version;
    __u32 max_factorial_input;
    __u32 max_roundtrip_len;
    __u32 reserved;
};

#define EDU_IOCTL_GET_INFO _IOR('E', 0, struct edu_info)



#endif //EDU_IOCTL_H