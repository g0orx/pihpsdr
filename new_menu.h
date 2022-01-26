extern GtkWidget *sub_menu;

extern void new_menu(void);

/*
extern GtkWidget* new_menu_init(int width,int height,GtkWidget *parent);
*/
extern void start_step(void);
extern void start_meter(void);
extern void start_band(void);
extern void start_bandstack(void);
extern void start_mode(void);
extern void start_filter(void);
extern void start_noise(void);
extern void start_encoder(void);
extern void start_vfo(int vfo);
extern void start_agc(void);
extern void start_store(void);
extern void start_rx(void);
extern void start_tx(void);
extern void start_diversity(void);
#ifdef PURESIGNAL
extern void start_ps(void);
#endif
#ifdef CLIENT_SERVER
extern void start_server(void);
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
  AGC_MENU,
  VFO_MENU
};

extern int active_menu;
