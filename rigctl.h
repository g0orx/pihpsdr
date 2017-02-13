#ifndef RIGCTL_H
#define RIGCTL_H

void launch_rigctl ();
int   rigctlGetMode();
char * rigctlGetFilter();
void set_freqB(long long);
void set_band(long long, int);
extern int cat_control;

#endif // RIGCTL_H
