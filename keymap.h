enum {
    MODE_UNK = 0,
    MODE_KEYBOARD,
    MODE_MOUSE,
    MODE_ANALOG,
    MODE_JOYSTICK,
};

struct rc_event {
    int		scan;
    __u16	type;
    __u16	code;
    __s32	value;
};

struct rc_event rc_events[] = {
    {  3, EV_KEY, 0, 0},
    {  4, EV_KEY, 0, 0},
    {  5, EV_KEY, 0, 0},
    {  7, EV_ABS, 0, 0},
    {  8, EV_ABS, 1, 0},
    {  9, EV_ABS, 2, 0},
    { 10, EV_ABS, 3, 0},
    { 15, EV_ABS, 8, 1},
    { 16, EV_ABS, 9, 1},
    { 17, EV_ABS, 10, 1},
    { 18, EV_ABS, 11, 1},
    { 19, EV_ABS, 12, 1},
    { 20, EV_ABS, 13, 1},
    { 21, EV_ABS, 14, 1},
    { 22, EV_ABS, 15, 1},
    { 23, EV_ABS, 16, 1},
    { 24, EV_ABS, 17, 1},
    { 25, EV_ABS, 18, 1},
    { 26, EV_ABS, 19, 1},
    {0, 0, 0, 0}
};
