#ifndef RIGCTL_H
#define RIGCTL_H

void launch_rigctl ();
int launch_serial ();
void disable_sreial ();

void  close_rigctl_ports ();
int   rigctlGetMode();
int   lookup_band(int);
char * rigctlGetFilter();
void set_freqB(long long);
int set_band(void *);
extern int cat_control;
int set_alc(gpointer);
extern int rigctl_busy;

extern int rigctl_port_base;
extern int rigctl_enable;


#endif // RIGCTL_H
