#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "update.h"
#include "version.h"

char new_version[128];

int check_update() {

  int rc=system("/usr/bin/wget -N https://github.com/g0orx/pihpsdr/raw/master/release/pihpsdr/latest");
  fprintf(stderr,"rc=%d\n", rc);
  if(rc==-1) {
    fprintf(stderr,"wget: errno=%d\n", errno);
  }

  fprintf(stderr,"check_version: current version is %s\n",version);
  strcpy(new_version,"");

  FILE* f=fopen("latest","r");
  if(f) {
    char *c=fgets(new_version,sizeof(new_version),f);
    fclose(f);
  } else {
    fprintf(stderr,"check_update: could not read latest version\n");
    return -1;
  }

  if(strlen(new_version)==0) {
    return -2;
  }

  if(strlen(new_version)>0) {
    if(new_version[strlen(new_version)-1]=='\n') {
      new_version[strlen(new_version)-1]='\0';
    }
  }

  fprintf(stderr,"check_version: latest version is %s\n",new_version);

  fprintf(stderr,"check_version: length of version=%ld length of new_version=%ld\n", (long)strlen(version), (long)strlen(new_version));
  rc=strcmp(version,new_version);

  return rc;
}

int load_update() {
  char command[1024];
  sprintf(command,"cd ..; /usr/bin/wget -N https://github.com/g0orx/pihpsdr/raw/master/release/pihpsdr_%s.tar",new_version);

fprintf(stderr,"load_update: %s\n",command);
  int rc=system(command);
  fprintf(stderr,"rc=%d\n", rc);
  if(rc==-1) {
    fprintf(stderr,"wget: errnoc=%d\n", errno);
  } else {
    fprintf(stderr,"load_update: %s loaded\n", new_version);
  }
  return rc;
}
