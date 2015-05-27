/* Example Sixaxis Bluetooth server using PF_BLUETOOTH sockets.

   Copyright (c) 2009 Jim Paris <jim@jtan.com>
   License: GPLv3
*/

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <err.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#include "bluetooth.h"
#include "l2cap.h"

#include "uinputdev.h"
#include "keymap.h"

#define MAX_CLIENTS 7

#define BTCTRL 17
#define BTDATA 19

struct hidfd {
  int ctrl;
  int data;
  bdaddr_t addr;
  int out;
  char outfname[128];
  unsigned char lastmsg[27];
};

struct hidfd client[MAX_CLIENTS];

static int mouse_emulation = 0;

struct repevent {
    int	fd;
    int	code;
    int	val;
};

struct repevent rep_evs[2];

static void timer_handler(int signum)
{
    int i;
    for (i = 0; i < 2; i++) {
	if (rep_evs[i].fd == -1)
	    continue;
	uinput_send(rep_evs[i].fd, EV_REL, rep_evs[i].code, rep_evs[i].val);
	uinput_send(rep_evs[i].fd, EV_SYN, SYN_REPORT, 0);
    
	if (rep_evs[i].val == 0)
	    rep_evs[i].fd = -1;
    }
}

static void exitfunc(int sig) {
  int i;
  for (i = 0; i < MAX_CLIENTS; i++) {
    if (bacmp(&client[i].addr, BDADDR_ANY) != 0) {
      unlink(client[i].outfname);
      uinput_close(client[i].out);
    }
  }
  exit(0);
}

static char *ba_str(const bdaddr_t * a) {
  static char str[2 * 6 + 5];

  sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
          a->b[5], a->b[4], a->b[3], a->b[2], a->b[1], a->b[0]);
  return str;
}

static int l2listen(int psm) {
  int fd;
  struct sockaddr_l2 addr;

  if ((fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1)
    return -1;
  memset(&addr, 0, sizeof(addr));
  addr.l2_family = AF_BLUETOOTH;
  addr.l2_bdaddr = *BDADDR_ANY;
  addr.l2_psm = htobs(psm);
  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
    return -1;
  if (listen(fd, 5) == -1)
    return -1;
  return fd;
}

static void initialize_sixaxis(struct hidfd *client, int i) {
  char buf[1024];

  unsigned char enable[] = {
    0x53,                              /* HIDP_TRANS_SET_REPORT | HIDP_DATA_RTYPE_FEATURE */
    0xf4, 0x42, 0x03, 0x00, 0x00
  };
  unsigned char setleds[] = {
    0x52,                              /* HIDP_TRANS_SET_REPORT | HIDP_DATA_RTYPE_OUTPUT */
    0x01,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1E,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char ledpattern[7] = {
    0x02, 0x04, 0x08, 0x10,
    0x12, 0x14, 0x18
  };

  printf("initializing controller %s\n", ba_str(&client[i].addr));

  /* enable reporting */
  send(client[i].ctrl, enable, sizeof(enable), 0);
  recv(client[i].ctrl, buf, sizeof(buf), 0);

  /* set LEDs */
  if (i < 7)
    setleds[11] = ledpattern[i];
  send(client[i].ctrl, setleds, sizeof(setleds), 0);
  recv(client[i].ctrl, buf, sizeof(buf), 0);
}

static int newclient(int psm, int s, struct hidfd *client) {
  int i;
  int fd;
  struct sockaddr_l2 addr;
  socklen_t len = sizeof(addr);

  /* Accept client */
  if ((fd = accept(s, (struct sockaddr *) &addr, &len)) == -1)
    return -1;

  /* Find this client */
  for (i = 0; i < MAX_CLIENTS; i++)
    if (bacmp(&client[i].addr, &addr.l2_bdaddr) == 0)
      break;
  if (i == MAX_CLIENTS) {
    /* Find empty client */
    for (i = 0; i < MAX_CLIENTS; i++)
      if (bacmp(&client[i].addr, BDADDR_ANY) == 0)
        break;
    if (i == MAX_CLIENTS) {
      errno = -ENOBUFS;
      return -1;
    }
    bacpy(&client[i].addr, &addr.l2_bdaddr);
  }

  /* Assign FD */
  if (psm == BTDATA) {
    client[i].data = fd;
    client->out = uinput_open(mouse_emulation);
    if (client->out == -1)
      err(1, "uinput_open(): %s", client->outfname);
    memset(client->lastmsg, 0xff, sizeof(client->lastmsg));
  }
  else
    client[i].ctrl = fd;

  /* Once both FDs are present, initialize the controller */
  if (client[i].data != -1 && client[i].ctrl != -1) {
    initialize_sixaxis(client, i);
  }

  return 0;
}

static struct rc_event *get_rc_event(int key)
{
    struct rc_event *tmp = rc_events;

    while (tmp->scan) {
	if (key == tmp->scan)
	    return tmp;
	tmp++;
    }
    
    return NULL;
}

static void process_data(int controller, struct hidfd *c, const unsigned char *data,
    int len) {
  int i, j, o;
  unsigned char d, mask;

  /* Try to get an output fd */
  if (c->out == -1)
    return;

  for (i=0; i<sizeof(c->lastmsg); i++) {
    d = data[i];
    /* center analog sticks */
    if (i>=7 && i<=10 && d>=0x70 && d<=0x90)
      d = 0x80;
    mask = c->lastmsg[i] ^ d;
    c->lastmsg[i] = d;
    if (!mask)
      continue;

    struct rc_event *ev = get_rc_event(i);

    if (ev) {
	switch (ev->type) {
	    case EV_KEY:
		o = (i - 3) * 8;
    		for (j=0; j<8; j++) {
    		    if (mask&1) {
    			if (mouse_emulation) {
    			    //fprintf(stderr, "button %d %d\n", j + o, d & 1);
    			    switch(j + o) {
    				case 14:
				    uinput_send(c->out, EV_KEY, BTN_LEFT, d & 1);
    				    break;
    				case 13:
				    uinput_send(c->out, EV_KEY, BTN_RIGHT, d & 1);
    				    break;
    				case 15:
				    uinput_send(c->out, EV_KEY, BTN_MIDDLE, d & 1);
    				    break;
    				default:
				    uinput_send(c->out, ev->type, BTN_JOYSTICK + j + o, d & 1);
    			    }
			    uinput_send(c->out, EV_SYN, SYN_REPORT, 0);
    			} else {
			    uinput_send(c->out, ev->type, BTN_JOYSTICK + j + o, d & 1);
			    uinput_send(c->out, EV_SYN, SYN_REPORT, 0);
			}
		    }
    		    mask >>= 1;
    		    d >>= 1;
		}
		break;
	    case EV_ABS:
		{
		    int val = ((int) d) * 257 - 32768;
    		    if (val == 128)
    			val = 0;
    		    if (mouse_emulation) {
    			switch (i) {
    			    case 7:
				uinput_send(c->out, EV_REL, REL_X, val / 16384);
				rep_evs[0].fd = c->out;
				rep_evs[0].code = REL_X;
				rep_evs[0].val = val / 4096;
				break;
    			    case 8:
				uinput_send(c->out, EV_REL, REL_Y, val / 16384);
				rep_evs[1].fd = c->out;
				rep_evs[1].code = REL_Y;
				rep_evs[1].val = val / 4096;
				break;
    			    default:
				uinput_send(c->out, ev->type, ev->code, val);
			}
			uinput_send(c->out, EV_SYN, SYN_REPORT, 0);
    		    } else {
			uinput_send(c->out, ev->type, ev->code, val);
			uinput_send(c->out, EV_SYN, SYN_REPORT, 0);
		    }
		}
		break;
	}
    }
  }

}

int main(int argc, char *argv[]) {
  struct hidfd server;
  struct pollfd pfd[MAX_CLIENTS + 2];
  int pfdindex[MAX_CLIENTS + 2];
  int i, nfd;
  unsigned char buf[1024];
  ssize_t len;

  mouse_emulation = 0;

  if (argc > 1) {
    if (!strcmp(argv[1], "-mouse"))
	mouse_emulation = 1;
  }

  signal(SIGINT, exitfunc);
  signal(SIGTERM, exitfunc);

  if (mouse_emulation) {
    int i;
    for (i = 0; i < 2; i++)
	rep_evs[i].fd = -1;
  }

  server.ctrl = l2listen(BTCTRL);
  if (server.ctrl < 0)
    err(1, "can't bind to psm %d", BTCTRL);
  server.data = l2listen(BTDATA);
  if (server.data < 0)
    err(1, "can't bind to psm %d", BTDATA);

  for (i = 0; i < MAX_CLIENTS; i++) {
    client[i].ctrl = client[i].data = -1;
    client[i].addr = *BDADDR_ANY;
  }

  printf("listening for connections\n");

  while (1) {
    memset(&pfd, 0, sizeof(pfd));

    /* Listen for inbound connections on control and data */
    pfd[0].fd = server.ctrl;
    pfd[0].events = POLLIN;
    pfd[1].fd = server.data;
    pfd[1].events = POLLIN;
    nfd = 2;

    /* Listen for client data or error */
    for (i = 0; i < MAX_CLIENTS; i++) {
      if (bacmp(&client[i].addr, BDADDR_ANY) == 0)
        continue;
      pfd[nfd].fd = client[i].data;
      pfd[nfd].events = POLLIN | POLLERR;
      pfdindex[nfd] = i;
      nfd++;
    }

    /* Wait for events */
    if (poll(pfd, nfd, -1) < 0)
      err(1, "poll");
    timer_handler(0);

//    int retval = 0;
//    do {
//	retval = poll(pfd, nfd, 250);
//	if (retval < 0)
//	    err(1, "poll");
//	timer_handler(0);
//    } while (retval == 0);

    /* Accept new connections */
    if (pfd[0].revents & POLLIN)
      if (newclient(BTCTRL, server.ctrl, client) == -1)
        warn("can't accept new ctrl client");
    if (pfd[1].revents & POLLIN)
      if (newclient(BTDATA, server.data, client) == -1)
        warn("can't accept new data client");

    /* Check clients */
    for (i = 2; i < nfd; i++) {
      int c = pfdindex[i];

      /* Closed connection */
      if (pfd[i].revents & POLLERR) {
        printf("closing client %s\n", ba_str(&client[c].addr));
        shutdown(client[c].ctrl, SHUT_RDWR);
        shutdown(client[c].data, SHUT_RDWR);
        close(client[c].ctrl);
        close(client[c].data);
        client[c].ctrl = client[c].data = -1;
        client[c].addr = *BDADDR_ANY;
        unlink(client->outfname);
        if (client[c].out) {
          close(client[c].out);
          client[c].out = -1;
        }
      }

      /* Incoming data */
      if (pfd[i].revents & POLLIN) {
        len = recv(client[c].data, buf, 1024, MSG_DONTWAIT);
        if (len > 0)
          process_data(c, &client[c], buf, len);
      }
    }
  }
}
