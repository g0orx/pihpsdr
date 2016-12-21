#ifndef RIGCTL_H
#define RIGCTL_H

void launch_rigctl ();
void parse_cmd(char *,int);
int   rigctlGetMode();
char * rigctlGetFilter();
void set_band(long long);

#endif // RIGCTL_H
