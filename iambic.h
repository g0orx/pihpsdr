#ifndef _IAMBIC_H
#define _IAMBIC_H

#ifdef LOCALCW
enum {
    CHECK = 0,
    STRAIGHT,
    PREDOT,
    SENDDOT,
    PREDASH,
    SENDDASH,
    DOTDELAY,
    DASHDELAY,
    LETTERSPACE,
    EXITLOOP
};

void keyer_event(int left, int state);
void keyer_update();
void keyer_close();
int  keyer_init();

#endif
#endif
