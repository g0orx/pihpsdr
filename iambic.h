#ifndef _IAMBIC_H
#define _IAMBIC_H

enum {
    CHECK = 0,
    SENDDOT,
    SENDDASH,
    DOTDELAY,
    DASHDELAY,
    LETTERSPACE,
    EXITLOOP
};

extern int keyer_out;
extern int key_state;

void keyer_event(int left, int state);
void keyer_update();
void keyer_close();
int  keyer_init();

#endif
