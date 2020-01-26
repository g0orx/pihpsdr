GtkWidget *sub_menu;

extern void new_menu();

/*
extern GtkWidget* new_menu_init(int width,int height,GtkWidget *parent);
*/
extern void start_step();
extern void start_meter();
extern void start_band();
extern void start_bandstack();
extern void start_mode();
extern void start_filter();
extern void start_noise();
extern void start_encoder();
extern void start_vfo();
extern void start_agc();
extern void start_store();
extern void start_rx();
extern void start_tx();
extern void start_diversity();
#ifdef PURESIGNAL
extern void start_ps();
#endif

extern void encoder_step(int encoder,int step);

extern int menu_active_receiver_changed(void *data);

enum {
  NO_MENU = 0,
  E2_MENU,
  E3_MENU,
  E4_MENU,
  E5_MENU,
  BAND_MENU,
  BANDSTACK_MENU,
  MODE_MENU,
  FILTER_MENU,
  NOISE_MENU,
  AGC_MENU
};

extern int active_menu;
