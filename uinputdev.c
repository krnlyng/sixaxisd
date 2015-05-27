/* 
 * Copyright (C) 2008 Alexander Chukov <sash@pdaXrom.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/uinput.h>

#include "uinputdev.h"

/* UInput */
static char *uinput_filename[] = {"/dev/uinput", "/dev/input/uinput",
                           "/dev/misc/uinput"};
#define UINPUT_FILENAME_COUNT (sizeof(uinput_filename)/sizeof(char *))

int uinput_open(int mode)
{
    unsigned int i;
    int fd = -1;
    struct uinput_user_dev dev;

    /* Open uinput device */
    for (i=0; i < UINPUT_FILENAME_COUNT; i++) {
	if ((fd = open(uinput_filename[i], O_RDWR)) >= 0) {
	    break;
	}
    }
    
    if (fd < 0) {
	fprintf(stderr, "Unable to open uinput\n");
	return -1;
    }

    memset(&dev, 0, sizeof(dev));

    if (mode)
	strncpy (dev.name, "sixaxis ps3 (mouse emulation)", UINPUT_MAX_NAME_SIZE);
    else
	strncpy (dev.name, "sixaxis ps3", UINPUT_MAX_NAME_SIZE);

    dev.id.version = 1;
    dev.id.bustype = BUS_BLUETOOTH;

    for(i = 0; i < 64 /*ABS_MAX*/;++i) {
	dev.absmax[i] =  32767;
	dev.absmin[i] = -32768;
    }

    ioctl (fd, UI_SET_EVBIT, EV_SYN);
    ioctl (fd, UI_SET_EVBIT, EV_ABS);
    ioctl (fd, UI_SET_EVBIT, EV_KEY);

    if (mode) {
	ioctl (fd, UI_SET_EVBIT, EV_REL);
        ioctl (fd, UI_SET_RELBIT, REL_X);
	ioctl (fd, UI_SET_RELBIT, REL_Y);
    }

    if (mode) {
	for (i = 0; i < 28; i++) {
	    if (ioctl (fd, UI_SET_ABSBIT, i) < 0) {
		fprintf(stderr, "error on uinput ioctl (UI_SET_ABSBIT)\n");
		return -1;
	    }
	}
    } else {
	ioctl (fd, UI_SET_ABSBIT, REL_X);
	ioctl (fd, UI_SET_ABSBIT, REL_Y);
	ioctl (fd, UI_SET_ABSBIT, REL_Z);
	ioctl (fd, UI_SET_ABSBIT, REL_RZ);
	for (i = 40; i < 64; i++) {
	    if (ioctl (fd, UI_SET_ABSBIT, i) < 0) {
		fprintf(stderr, "error on uinput ioctl (UI_SET_ABSBIT)\n");
		return -1;
	    }
	}
    }

    for (i = 0; i < 19; i++) {
	if (ioctl(fd, UI_SET_KEYBIT, BTN_JOYSTICK + i) < 0) {
	    fprintf(stderr, "error on uinput ioctl (UI_SET_KEYBIT)\n");
	    return -1;
	}
    }

    if (mode) {
        ioctl (fd, UI_SET_KEYBIT, BTN_MOUSE); 
        ioctl (fd, UI_SET_KEYBIT, BTN_LEFT); 
        ioctl (fd, UI_SET_KEYBIT, BTN_MIDDLE); 
        ioctl (fd, UI_SET_KEYBIT, BTN_RIGHT); 
        ioctl (fd, UI_SET_KEYBIT, BTN_FORWARD); 
        ioctl (fd, UI_SET_KEYBIT, BTN_BACK);
	ioctl (fd, UI_SET_KEYBIT, BTN_TOUCH); 
    }

    if (write(fd, &dev, sizeof(dev)) != sizeof(dev)) {
	fprintf(stderr, "Error on uinput device setup\n");
	close(fd);
	return -1;
    }

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
	fprintf(stderr, "Error on uinput dev create");
	close(fd);
	return -1;
    }

    return fd;
}

int uinput_close(int fd)
{
    if (ioctl(fd, UI_DEV_DESTROY) < 0) {
	fprintf(stderr, "Error on uinput ioctl (UI_DEV_DESTROY)\n");
    }

    if (close(fd)) {
	fprintf(stderr, "Error on uinput close");
	return -1;
    }

    return 0;
}

int uinput_send(int fd, __u16 type, __u16 code, __s32 value)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    gettimeofday(&(event.time), NULL);

    if (write(fd, &event, sizeof(event)) != sizeof(event)) {
	fprintf(stderr, "Error on send_event\n");
	return -1;
    }

    return 0;
}
