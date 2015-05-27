#ifndef __UINPUTDEV_H__
#define __UINPUTDEV_H__

#include <linux/input.h>
#include <linux/uinput.h>

#if defined (__cplusplus)
extern "C" {
#endif

int uinput_open(int mode);

int uinput_close(int fd);

int uinput_send(int fd, __u16 type, __u16 code, __s32 value);

#if defined (__cplusplus)
}
#endif

#endif
