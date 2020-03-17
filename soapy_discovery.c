/* Copyright (C)
* 2019 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include "discovered.h"
#include "soapy_discovery.h"

static int rtlsdr_count=0;

static void get_info(char *driver) {
  size_t rx_rates_length, tx_rates_length, rx_gains_length, tx_gains_length, ranges_length, rx_antennas_length, tx_antennas_length, rx_bandwidth_length, tx_bandwidth_length;
  int i;
  SoapySDRKwargs args={};
  int software_version=0;
  int rtlsdr_val=0;
  char fw_version[64];
  char gw_version[64];
  char hw_version[64];
  char p_version[64];

  fprintf(stderr,"soapy_discovery: get_info: %s\n", driver);

  strcpy(fw_version,"");
  strcpy(gw_version,"");
  strcpy(hw_version,"");
  strcpy(p_version,"");

  SoapySDRKwargs_set(&args, "driver", driver);
  if(strcmp(driver,"rtlsdr")==0) {
    char count[16];
    sprintf(count,"%d",rtlsdr_count);
    SoapySDRKwargs_set(&args, "rtl", count);
    rtlsdr_val=rtlsdr_count;
    rtlsdr_count++;
  }
  SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
  SoapySDRKwargs_clear(&args);
  software_version=0;

  char *driverkey=SoapySDRDevice_getDriverKey(sdr);
  fprintf(stderr,"DriverKey=%s\n",driverkey);

  char *hardwarekey=SoapySDRDevice_getHardwareKey(sdr);
  fprintf(stderr,"HardwareKey=%s\n",hardwarekey);

  SoapySDRKwargs info=SoapySDRDevice_getHardwareInfo(sdr);
  for(i=0;i<info.size;i++) {
    fprintf(stderr,"soapy_discovery: hardware info key=%s val=%s\n",info.keys[i], info.vals[i]);
    if(strcmp(info.keys[i],"firmwareVersion")==0) {
      strcpy(fw_version,info.vals[i]);
    }
    if(strcmp(info.keys[i],"gatewareVersion")==0) {
      strcpy(gw_version,info.vals[i]);
      software_version=(int)(atof(info.vals[i])*100.0);
    }
    if(strcmp(info.keys[i],"hardwareVersion")==0) {
      strcpy(hw_version,info.vals[i]);
    }
    if(strcmp(info.keys[i],"protocolVersion")==0) {
      strcpy(p_version,info.vals[i]);
    }
  }

  size_t rx_channels=SoapySDRDevice_getNumChannels(sdr, SOAPY_SDR_RX);
  fprintf(stderr,"Rx channels: %ld\n",rx_channels);
  for(int i=0;i<rx_channels;i++) {
    fprintf(stderr,"Rx channel full duplex: channel=%d fullduplex=%d\n",i,SoapySDRDevice_getFullDuplex(sdr, SOAPY_SDR_RX, i));
  }

  size_t tx_channels=SoapySDRDevice_getNumChannels(sdr, SOAPY_SDR_TX);
  fprintf(stderr,"Tx channels: %ld\n",tx_channels);
  for(int i=0;i<tx_channels;i++) {
    fprintf(stderr,"Tx channel full duplex: channel=%d fullduplex=%d\n",i,SoapySDRDevice_getFullDuplex(sdr, SOAPY_SDR_TX, i));
  }


  int sample_rate=0;
  SoapySDRRange *rx_rates=SoapySDRDevice_getSampleRateRange(sdr, SOAPY_SDR_RX, 0, &rx_rates_length);
  fprintf(stderr,"Rx sample rates: ");
  for (size_t i = 0; i < rx_rates_length; i++) {
    fprintf(stderr,"%f -> %f (%f),", rx_rates[i].minimum, rx_rates[i].maximum, rx_rates[i].minimum/48000.0);
    if(sample_rate==0) {
      if(rx_rates[i].minimum==rx_rates[i].maximum) {
        if(((int)rx_rates[i].minimum%48000)==0) {
          sample_rate=(int)rx_rates[i].minimum;
        }
      } else {
        if((384000.0>=rx_rates[i].minimum) && (384000<=(int)rx_rates[i].maximum)) {
          sample_rate=384000;
        }
        if((768000.0>=rx_rates[i].minimum) && (768000<=(int)rx_rates[i].maximum)) {
          sample_rate=768000;
        }
      }
    }
  }
  fprintf(stderr,"\n");
  free(rx_rates);

  if(strcmp(driver,"lime")==0) {
    sample_rate=768000;
  } else if(strcmp(driver,"plutosdr")==0) {
    sample_rate=768000;
  } else if(strcmp(driver,"rtlsdr")==0) {
    sample_rate=1024000;
  } else {
    sample_rate=1024000;
  }

  fprintf(stderr,"sample_rate selected %d\n",sample_rate);

  SoapySDRRange *tx_rates=SoapySDRDevice_getSampleRateRange(sdr, SOAPY_SDR_TX, 1, &tx_rates_length);
  fprintf(stderr,"Tx sample rates: ");
  for (size_t i = 0; i < tx_rates_length; i++) {
    fprintf(stderr,"%f -> %f (%f),", tx_rates[i].minimum, tx_rates[i].maximum, tx_rates[i].minimum/48000.0);
  }
  fprintf(stderr,"\n");
  free(tx_rates);

  double *bandwidths=SoapySDRDevice_listBandwidths(sdr, SOAPY_SDR_RX, 0, &rx_bandwidth_length);
  fprintf(stderr,"Rx bandwidths: ");
  for (size_t i = 0; i < rx_bandwidth_length; i++) {
    fprintf(stderr,"%f, ", bandwidths[i]);
  }
  fprintf(stderr,"\n");
  free(bandwidths);

  bandwidths=SoapySDRDevice_listBandwidths(sdr, SOAPY_SDR_TX, 0, &tx_bandwidth_length);
  fprintf(stderr,"Tx bandwidths: ");
  for (size_t i = 0; i < tx_bandwidth_length; i++) {
    fprintf(stderr,"%f, ", bandwidths[i]);
  }
  fprintf(stderr,"\n");
  free(bandwidths);

  double bandwidth=SoapySDRDevice_getBandwidth(sdr, SOAPY_SDR_RX, 0);
  fprintf(stderr,"RX0: bandwidth=%f\n",bandwidth);

  bandwidth=SoapySDRDevice_getBandwidth(sdr, SOAPY_SDR_TX, 0);
  fprintf(stderr,"TX0: bandwidth=%f\n",bandwidth);

  SoapySDRRange *ranges = SoapySDRDevice_getFrequencyRange(sdr, SOAPY_SDR_RX, 0, &ranges_length);
  fprintf(stderr,"Rx freq ranges: ");
  for (size_t i = 0; i < ranges_length; i++) fprintf(stderr,"[%f Hz -> %f Hz step=%f], ", ranges[i].minimum, ranges[i].maximum,ranges[i].step);
  fprintf(stderr,"\n");

  char** rx_antennas = SoapySDRDevice_listAntennas(sdr, SOAPY_SDR_RX, 0, &rx_antennas_length);
  fprintf(stderr, "Rx antennas: ");
  for (size_t i = 0; i < rx_antennas_length; i++) fprintf(stderr, "%s, ", rx_antennas[i]);
  fprintf(stderr,"\n");

  char** tx_antennas = SoapySDRDevice_listAntennas(sdr, SOAPY_SDR_TX, 0, &tx_antennas_length);
  fprintf(stderr, "Tx antennas: ");
  for (size_t i = 0; i < tx_antennas_length; i++) fprintf(stderr, "%s, ", tx_antennas[i]);
  fprintf(stderr,"\n");

  char **rx_gains = SoapySDRDevice_listGains(sdr, SOAPY_SDR_RX, 0, &rx_gains_length);

  gboolean has_automatic_gain=SoapySDRDevice_hasGainMode(sdr, SOAPY_SDR_RX, 0);
  fprintf(stderr,"has_automaic_gain=%d\n",has_automatic_gain);

  gboolean has_automatic_dc_offset_correction=SoapySDRDevice_hasDCOffsetMode(sdr, SOAPY_SDR_RX, 0);
  fprintf(stderr,"has_automaic_dc_offset_correction=%d\n",has_automatic_dc_offset_correction);

  char **tx_gains = SoapySDRDevice_listGains(sdr, SOAPY_SDR_TX, 1, &tx_gains_length);

  size_t formats_length;
  char **formats = SoapySDRDevice_getStreamFormats(sdr,SOAPY_SDR_RX,0,&formats_length);
  fprintf(stderr, "Rx formats: ");
  for (size_t i = 0; i < formats_length; i++) fprintf(stderr, "%s, ", formats[i]);
  fprintf(stderr,"\n");

  fprintf(stderr,"float=%lu double=%lu\n",sizeof(float),sizeof(double));

  if(devices<MAX_DEVICES) {
    discovered[devices].device=SOAPYSDR_USB_DEVICE;
    discovered[devices].protocol=SOAPYSDR_PROTOCOL;
    strcpy(discovered[devices].name,driver);
    discovered[devices].supported_receivers=rx_channels;
    discovered[devices].supported_transmitters=tx_channels;
    discovered[devices].adcs=rx_channels;
    discovered[devices].dacs=tx_channels;
    discovered[devices].status=STATE_AVAILABLE;
    discovered[devices].software_version=software_version;
    discovered[devices].frequency_min=ranges[0].minimum;
    discovered[devices].frequency_max=ranges[0].maximum;
    strcpy(discovered[devices].info.soapy.driver_key,driverkey);
    strcpy(discovered[devices].info.soapy.hardware_key,hardwarekey);
    discovered[devices].info.soapy.sample_rate=sample_rate;
    if(strcmp(driver,"rtlsdr")==0) {
      discovered[devices].info.soapy.rtlsdr_count=rtlsdr_val;
    } else {
      discovered[devices].info.soapy.rtlsdr_count=0;
    }
    if(strcmp(driver,"lime")==0) {
      sprintf(discovered[devices].info.soapy.version,"fw=%s gw=%s hw=%s p=%s", fw_version, gw_version, hw_version, p_version);
    } else {
      strcpy(discovered[devices].info.soapy.version,"");
    }
    discovered[devices].info.soapy.rx_channels=rx_channels;
    discovered[devices].info.soapy.rx_gains=rx_gains_length;
    discovered[devices].info.soapy.rx_gain=rx_gains;
    discovered[devices].info.soapy.rx_range=malloc(rx_gains_length*sizeof(SoapySDRRange));
fprintf(stderr,"Rx gains: \n");
    for (size_t i = 0; i < rx_gains_length; i++) {
      fprintf(stderr,"%s ", rx_gains[i]);
      SoapySDRRange rx_range=SoapySDRDevice_getGainElementRange(sdr, SOAPY_SDR_RX, 0, rx_gains[i]);
      fprintf(stderr,"%f -> %f step=%f\n",rx_range.minimum,rx_range.maximum,rx_range.step);
      discovered[devices].info.soapy.rx_range[i]=rx_range;
    }
    discovered[devices].info.soapy.rx_has_automatic_gain=has_automatic_gain;
    discovered[devices].info.soapy.rx_has_automatic_dc_offset_correction=has_automatic_dc_offset_correction;
    discovered[devices].info.soapy.rx_antennas=rx_antennas_length;
    discovered[devices].info.soapy.rx_antenna=rx_antennas;

    discovered[devices].info.soapy.tx_channels=tx_channels;
    discovered[devices].info.soapy.tx_gains=tx_gains_length;
    discovered[devices].info.soapy.tx_gain=tx_gains;
    discovered[devices].info.soapy.tx_range=malloc(tx_gains_length*sizeof(SoapySDRRange));
fprintf(stderr,"Tx gains: \n");
    for (size_t i = 0; i < tx_gains_length; i++) {
      fprintf(stderr,"%s ", tx_gains[i]);
      SoapySDRRange tx_range=SoapySDRDevice_getGainElementRange(sdr, SOAPY_SDR_TX, 1, tx_gains[i]);
      fprintf(stderr,"%f -> %f step=%f\n",tx_range.minimum,tx_range.maximum,tx_range.step);
      discovered[devices].info.soapy.tx_range[i]=tx_range;
    }
    discovered[devices].info.soapy.tx_antennas=tx_antennas_length;
    discovered[devices].info.soapy.tx_antenna=tx_antennas;
    devices++;
  }


  free(ranges);

}

void soapy_discovery() {
  size_t length;
  int i,j;
  SoapySDRKwargs args={};

fprintf(stderr,"soapy_discovery\n");
  rtlsdr_count=0;
  SoapySDRKwargs *results = SoapySDRDevice_enumerate(NULL, &length);
fprintf(stderr,"soapy_discovery: length=%d\n",length);
  for (i = 0; i < length; i++) {
    for (size_t j = 0; j < results[i].size; j++) {
      if(strcmp(results[i].keys[j],"driver")==0 && strcmp(results[i].vals[j],"audio")!=0) {
        get_info(results[i].vals[j]);
      }
    }
  }
  SoapySDRKwargsList_clear(results, length);
}
