#include <stdio.h>
#include <SoapySDR/Device.h>
#include "lime_discovery.h"

void lime_discovery() {
  SoapySDRKwargs args;
  size_t length;
  int i;
  args.size=0;
  SoapySDRKwargs *devices=SoapySDRDevice_enumerate(&args, &length);

fprintf(stderr,"lime_discovery: length=%d devices->size=%d\n",length,devices->size);

  for(i=0;i<length;i++) {
fprintf(stderr,"lime_discovery: key=%s val=%s\n",devices->keys[i], devices->vals[i]);
  }

  fprintf(stderr,"lime_discovery found %d devices\n",length);
}
