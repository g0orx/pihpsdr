
#include <gtk/gtk.h>

#include "radio.h"
#include "vox.h"
#include "vfo.h"

static guint vox_timeout;

static int vox_timeout_cb(gpointer data) {
fprintf(stderr,"vox_timeout_cb: setVox: 0\n");
  setVox(0);
  g_idle_add(vfo_update,NULL);
  return FALSE;
}

void update_vox(double *in,int length) {
  // assumes in is interleaved left and right channel with length samples
  int previous_vox=vox;
  int i;
  double peak=0.0;
  double sample;
  for(i=0;i<length;i++) {
    sample=in[(i*2)+0];
    if(sample<0.0) {
      sample=-sample;
    }
    if(sample>peak) {
      peak=sample;
    }
  }
  double threshold=vox_threshold;
  if(mic_boost && !local_microphone) {
    threshold=vox_threshold*vox_gain;
  }
  if(peak>threshold) {
fprintf(stderr,"update_vox: threshold=%f\n",threshold);
    if(previous_vox) {
fprintf(stderr,"update_vox: g_source_remove: vox_timeout\n");
      g_source_remove(vox_timeout);
    } else {
fprintf(stderr,"update_vox: setVox: 1\n");
      setVox(1);
    }
fprintf(stderr,"g_timeout_add: %d\n",(int)vox_hang);
    vox_timeout=g_timeout_add((int)vox_hang,vox_timeout_cb,NULL);
  }
  if(vox!=previous_vox) {
    g_idle_add(vfo_update,NULL);
  }
}

void vox_cancel() {
  if(vox) {
fprintf(stderr,"vox_cancel: g_source_remove: vox_timeout\n");
    g_source_remove(vox_timeout);
fprintf(stderr,"vox_cancel: setVox: 0\n");
    setVox(0);
  }
}
