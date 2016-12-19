#ifndef _IAMBIC_H
#define _IAMBIC_H

enum {
    CHECK = 0,
    PREDOT,
    PREDASH,
    SENDDOT,
    SENDDASH,
    DOTDELAY,
    DASHDELAY,
    DOTHELD,
    DASHHELD,
    LETTERSPACE,
    EXITLOOP
};

extern int cwl_state;
extern int cwr_state;
extern int keyer_out;

extern int key_state;
extern int *kdot;
extern int *kdash;

void keyer_event(int gpio, int level);
void keyer_update();
void keyer_close();

#endif
