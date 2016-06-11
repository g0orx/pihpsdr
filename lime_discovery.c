#include <stdio.h>
#include <string.h>
#include <SoapySDR/Device.h>
#include "discovered.h"
#include "lime_discovery.h"

void lime_discovery() {
  SoapySDRKwargs args;
  size_t length;
  int i;
  args.size=0;
  SoapySDRKwargs *devs=SoapySDRDevice_enumerate(&args, &length);

fprintf(stderr,"lime_discovery: length=%ld devs->size=%ld\n",length,devs->size);

  for(i=0;i<devs->size;i++) {
fprintf(stderr,"lime_discovery:device key=%s val=%s\n",devs->keys[i], devs->vals[i]);
    if(strcmp(devs->keys[i],"name")==0) {
      discovered[devices].protocol=LIMESDR_PROTOCOL;
      discovered[devices].device=LIMESDR_USB_DEVICE;
      strcpy(discovered[devices].name,devs->vals[i]);
      discovered[devices].status=STATE_AVAILABLE;
      discovered[devices].info.soapy.args=devs;
      devices++;
    }
  }

/*
  SoapySDRDevice *device=SoapySDRDevice_make(devs);
  if(device==NULL) {
    fprintf(stderr,"SoapySDRDevice_make failed: %s\n",SoapySDRDevice_lastError());
    return;
  }

  SoapySDRKwargs info=SoapySDRDevice_getHardwareInfo(device);
  int version=0;
  for(i=0;i<info.size;i++) {
fprintf(stderr,"lime_discovery: info key=%s val=%s\n",info.keys[i], info.vals[i]);
    if(strcmp(info.keys[i],"firmwareVersion")==0) {
      version+=atoi(info.vals[i])*100;
    }
    if(strcmp(info.keys[i],"hardwareVersion")==0) {
      version+=atoi(info.vals[i])*10;
    }
    if(strcmp(info.keys[i],"protocolVersion")==0) {
      version+=atoi(info.vals[i]);
    }
  }
  
  discovered[devices].software_version=version;
*/

/*
  fprintf(stderr,"SampleRates\n");
  int rates;
  double *rate;
  rate=SoapySDRDevice_listSampleRates(device,SOAPY_SDR_RX,0,&rates);
  for(i=0;i<rates;i++) {
    fprintf(stderr,"rate=%f\n",rate[i]);
  }

  fprintf(stderr,"Bandwidths\n");
  int bandwidths;
  double *bandwidth;
  bandwidth=SoapySDRDevice_listSampleRates(device,SOAPY_SDR_RX,0,&bandwidths);
  for(i=0;i<bandwidths;i++) {
    fprintf(stderr,"bandwidth=%f\n",bandwidth[i]);
  }

  fprintf(stderr,"Antennas\n");
  int antennas;
  char **antenna;
  antenna=SoapySDRDevice_listAntennas(device,SOAPY_SDR_RX,0,&antennas);
  for(i=0;i<antennas;i++) {
    fprintf(stderr,"antenna=%s\n",antenna[i]);
  }

  char *ant=SoapySDRDevice_getAntenna(device,SOAPY_SDR_RX,0);
  fprintf(stderr,"selected antenna=%s\n",ant);

  SoapySDRDevice_unmake(device);

*/
  fprintf(stderr,"lime_discovery found %d devices\n",length);
}
