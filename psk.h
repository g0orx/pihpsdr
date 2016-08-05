#define PSK_BUFFER_SIZE 1024

#define PSK_RX_TEXT_SIZE 2048
extern char psk_rx_text_data[PSK_RX_TEXT_SIZE];

extern GtkWidget *init_psk();
extern void close_psk();
extern void psk_set_frequency(int f);
extern int psk_get_frequency();
extern int psk_get_signal_level();
extern int psk_demod(double sample);
