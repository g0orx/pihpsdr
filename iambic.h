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

void keyer_event(int left, int state);
void keyer_update(void);
void keyer_close(void);
int  keyer_init(void);

#endif
