#ifndef _BEEP_H
#define _BEEP_H

extern double beep_freq;

void beep_vol(long volume);
void beep_mute(int mute);
void beep_init(void);
void beep_close(void);

#endif
