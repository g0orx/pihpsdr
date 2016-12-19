#define MAX_RECEIVERS 2

struct _receiver {
  int sample_rate;
  int buffer_size;
  int band;
  int bandstack;
  int mode;
  int filter;
  int audio;
} rx[MAX_RECEIVERS];


