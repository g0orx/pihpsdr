#ifndef _IAMBIC_H
#define _IAMBIC_H

extern int cwl_state;
extern int cwr_state;
extern int keyer_out;

void keyer_event(int gpio, int level);
void keyer_update();
void keyer_close();

#endif
