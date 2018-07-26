/* Copyright (C)
* 2017 - Markus Großer, DL8GM
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

// DL1YCF: we provide a stripped-down version not relying on AVAHI.
// this is compiled when defining NO_AVAHI


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <string.h>

#include "discovered.h"
#include "radio.h"

#ifndef NO_AVAHI
#include <avahi-gobject/ga-client.h>
#include <avahi-gobject/ga-service-browser.h>
#include <avahi-gobject/ga-service-resolver.h>
#endif

// As we only run in the GTK+ main event loop, which is single-threaded and
// non-preemptive, we shouldn't need any additional synchronisation mechanisms.
static bool discovery_done = FALSE;
static int pending_callbacks = 0;
static struct ifaddrs *ifaddrs = NULL;
static bool curl_initialised = FALSE;
extern void status_text(const char *);

#define ERROR_PREFIX "stemlab_discovery: "

//
// A bunch of callback-routines that use avahi, and are only needed by the
// avahi-dependent version of stemlab_discovery(). These are only compiled
// in the avahi case.
//
#ifndef NO_AVAHI
static void resolver_found_cb(GaServiceResolver *resolver, AvahiIfIndex if_index,
    GaProtocol protocol, gchar *name, gchar *service, gchar *domain,
    gchar *hostname, AvahiAddress *address, gint port, AvahiStringList *txt,
    GaLookupResultFlags flags) {
  pending_callbacks--;
  // RedPitaya's MAC address block starts with 00:26:32
  unsigned char mac_address[6] = {0x00, 0x26, 0x32};
  if (3 != sscanf(name, "rp-%2hhx%2hhx%2hhx HTTP",
      &mac_address[3], &mac_address[4], &mac_address[5])) {
    return;
  }

  // Avahi uses interface indices, as defined in net/if.h
  // Everything else in this codebase however uses ifaddrs.h, so we need to
  // "translate" between the two (aka look through the latter until we found
  // one with the correct name)
  char if_name[IF_NAMESIZE] = {0};
  const char *indextoname_res = if_indextoname(if_index, if_name);
  if (indextoname_res == NULL) {
    const int error_code = errno;
    perror(ERROR_PREFIX "Failed translating interface index to name\n");
    return;
  }
  if (ifaddrs == NULL) {
    const int getifaddrs_res = getifaddrs(&ifaddrs);
    if (getifaddrs_res == -1) {
      const int error_code = errno;
      perror(ERROR_PREFIX "Failed enumerating interfaces");
      ifaddrs = NULL;
      return;
    }
  }
  struct ifaddrs *current_if = ifaddrs;
  while (current_if != NULL) {
    if (current_if->ifa_addr != NULL
      && current_if->ifa_addr->sa_family == AF_INET
      && (current_if->ifa_flags & IFF_UP) != 0
      && (current_if->ifa_flags & IFF_RUNNING) != 0
      && strcmp(current_if->ifa_name, if_name) == 0) {
      break;
    }
    current_if = current_if->ifa_next;
  }
  if (current_if == NULL) {
    fprintf(stderr, ERROR_PREFIX "Did not find interface given by Avahi\n");
    return;
    // ifaddrs struct will be cleared up in cache_exhausted_cb, so no need
    // to do it here
  }

  // Both Avahi as well as the standard headers use network byte order, so
  // there is no necessity to do any endianness conversion here
  struct in_addr ip_address = { .s_addr = address->data.ipv4.address };
  CURL *curl_handle = curl_easy_init();
  if (curl_handle == NULL) {
    fprintf(stderr, ERROR_PREFIX "Failed to create cURL handler\n");
  }
  // This is just a dummy string to ensure the buffer has the correct length
  char app_list_url[] = "http://123.123.123.123/bazaar?apps=";
  int app_list = 0;
  sprintf(app_list_url, "http://%s/bazaar?apps=", inet_ntoa(ip_address));
#define check_curl(description) do { \
  if (curl_error != CURLE_OK) { \
    fprintf(stderr, ERROR_PREFIX description ": %s\n", \
        curl_easy_strerror(curl_error)); \
    curl_easy_cleanup(curl_handle); \
    return; \
  } \
} while (0)
  CURLcode curl_error = CURLE_OK;
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_URL, app_list_url);
  check_curl("Failed setting cURL URL");
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, app_list_cb);
  check_curl("Failed setting cURL callback");
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &app_list);
  check_curl("Failed setting cURL callback parameter");
  // This is a blocking callback, so we don't need to do any additional shenanigans here
  curl_easy_perform(curl_handle);
  check_curl("Failed getting app list");
#undef check_curl
  curl_easy_cleanup(curl_handle);

  DISCOVERED *device = &discovered[devices];
  devices++;
  device->protocol = STEMLAB_PROTOCOL;
  device->device = DEVICE_METIS;
  strcpy(device->name, "STEMlab");
  device->software_version = app_list;
  device->status = STATE_AVAILABLE;
  memcpy(device->info.network.mac_address, mac_address, 6);
  // Since elsewhere the pointers are just casted anyways, I don't really see
  // a point in solving this in any other way
  device->info.network.address_length = sizeof(struct sockaddr_in);
  device->info.network.address.sin_family = AF_INET;
  device->info.network.address.sin_addr = ip_address;
  device->info.network.address.sin_port = htons(1024);
  device->info.network.interface_length = sizeof(struct sockaddr_in);
  device->info.network.interface_address = * (struct sockaddr_in *) current_if->ifa_addr;
  device->info.network.interface_netmask = * (struct sockaddr_in *) current_if->ifa_netmask;
  strcpy(device->info.network.interface_name, if_name);
}

void resolver_failure_cb(GaServiceResolver *resolver, GError *error, gpointer data) {
  pending_callbacks--;
}

static void new_service_cb(GaServiceBrowser *browser, gint if_index, GaProtocol protocol,
    gchar *name, gchar *service, gchar *domain, GaLookupResultFlags flags,
    GaClient *client) {
  // We aren't actually interested in it here, but otherwise we can't
  // differentiate between matching success and matching failure, since we'd
  // always get 0 matched and assigned arguments
  int mac_buffer = 0;
  if (1 != sscanf(name, "rp-%6x HTTP", &mac_buffer)) {
    return;
  }
  if (devices >= MAX_DEVICES) {
    fprintf(stderr, ERROR_PREFIX "Maximum number of devices (%d) reached\n",
        MAX_DEVICES);
  }
  if (protocol != AVAHI_PROTO_INET) {
    fprintf(stderr, ERROR_PREFIX "found %.9s via IPv6, skipping ...\n", name);
    return;
  }
  GaServiceResolver *resolver = ga_service_resolver_new(if_index, protocol,
      name, service, domain, AVAHI_PROTO_INET, GA_LOOKUP_NO_FLAGS);
  if (resolver == NULL) {
    fprintf(stderr, ERROR_PREFIX "Failed creating Avahi resolver for %.9s\n", name);
    return;
  }
  GError *attachment_error = NULL;
  if (!ga_service_resolver_attach(resolver, client, &attachment_error)) {
    fprintf(stderr, ERROR_PREFIX "Failed attaching resolver to Avahi client: %s\n",
        attachment_error == NULL ? "(Unknown Error)" : attachment_error->message);
    return;
  }
  const gulong resolver_found_handler =
      g_signal_connect(resolver, "found", G_CALLBACK(resolver_found_cb), NULL);
  if (resolver_found_handler <= 0) {
    fprintf(stderr, ERROR_PREFIX "Failed installing resolver \"found\" signal handler\n");
    return;
  }
  if (g_signal_connect(resolver, "failure", G_CALLBACK(resolver_failure_cb), NULL) <= 0) {
    fprintf(stderr, ERROR_PREFIX "Failed installing resolver \"failure\" signal handler\n");
    g_signal_handler_disconnect(resolver, resolver_found_handler);
    return;
  }
  pending_callbacks++;
}

static void cache_exhausted_cb(GaServiceBrowser *browser, gpointer data) {
  if (ifaddrs != NULL) {
    freeifaddrs(ifaddrs);
    ifaddrs = NULL;
  }
  discovery_done = TRUE;
}
#endif

//
// Some callback routines and functions that do not depend on avahi
// and are compiled in either case.
//
static size_t app_list_cb(void *buffer, size_t size, size_t nmemb, void *data) {
  // cURL does *not* make any guarantees for this data to be the complete
  // However, as the STEMlab answers in one big chunk, we just hope for the
  // answer to be the complete json object, and avoid the hassle of manually
  // building up our buffer.
  int *software_version = (int*) data;
  // This is not 100% clean, but avoids requiring in a json library dependency
  const gchar *pavel_rx_json = "\"sdr_receiver_hpsdr\":";
  if (g_strstr_len(buffer, size*nmemb, pavel_rx_json) != NULL) {
    *software_version |= STEMLAB_PAVEL_RX;
  }
  const gchar *pavel_trx_json = "\"sdr_transceiver_hpsdr\":";
  if (g_strstr_len(buffer, size*nmemb, pavel_trx_json) != NULL) {
    *software_version |= STEMLAB_PAVEL_TRX;
  }
  const gchar *rp_trx_json = "\"stemlab_sdr_transceiver_hpsdr\":";
  if (g_strstr_len(buffer, size*nmemb, rp_trx_json) != NULL) {
    *software_version |= STEMLAB_RP_TRX;
  }
  const gchar *hamlab_trx_json = "\"hamlab_sdr_transceiver_hpsdr\":";
  if (g_strstr_len(buffer, size*nmemb, hamlab_trx_json) != NULL) {
    *software_version |= HAMLAB_RP_TRX;
  }
  // Returning the total amount of bytes "processed" to signal cURL that we
  // are done without any errors
  return size * nmemb;
}

// This is essentially a no-op curl callback.
// Its main purpose is to prevent the status message going to stderr
static size_t app_start_callback(void *buffer, size_t size, size_t nmemb, void *data) {
  if (strncmp(buffer, "{\"status\":\"OK\"}", size*nmemb) != 0) {
    fprintf(stderr, "stemlab_start: Receiver error from STEMlab\n");
    return 0;
  }
  return size * nmemb;
}

int stemlab_start_app(const char * const app_id) {
  // Dummy string, using the longest possible app id
  char app_start_url[] = "http://123.123.123.123/bazaar?start=stemlab_sdr_transceiver_hpsdr_headroom_max";
  //
  // If there is already an SDR application running on the RedPitaya,
  // starting the SDR app might lead to an unpredictable state, unless
  // the "killall" command from stop.sh is included in start.sh but this
  // is not done at the factory.
  // Therefore, we first stop the program (this essentially includes the
  // command "killall sdr_transceiver_hpsdr") and then start it.
  // We return with value 0 if everything went OK, else we return -1.
  // This is so because in the NO_AVAHI case, we can in principle recover
  // from a failure here.

  CURL *curl_handle = curl_easy_init();
  CURLcode curl_error = CURLE_OK;

  if (curl_handle == NULL) {
    fprintf(stderr, "stemlab_start: Failed to create cURL handle\n");
    return -1;
  }

#define check_curl(description) do { \
  if (curl_error != CURLE_OK) { \
    fprintf(stderr, "stemlab_start: " description ": %s\n", \
        curl_easy_strerror(curl_error)); \
     return -1; \
  } \
}  while (0);

  sprintf(app_start_url, "http://%s/bazaar?stop=%s",
          inet_ntoa(radio->info.network.address.sin_addr),
          app_id);
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_URL, app_start_url);
  check_curl("Failed setting cURL URL");
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, app_start_callback);
  check_curl("Failed install cURL callback");
  curl_error = curl_easy_perform(curl_handle);
  check_curl("Failed to stop app");

  sprintf(app_start_url, "http://%s/bazaar?start=%s",
          inet_ntoa(radio->info.network.address.sin_addr),
          app_id);
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_URL, app_start_url);
  check_curl("Failed setting cURL URL");
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, app_start_callback);
  check_curl("Failed install cURL callback");
  curl_error = curl_easy_perform(curl_handle);
  check_curl("Failed to start app");

#undef check_curl

  curl_easy_cleanup(curl_handle);
  // Since the SDR application is now running, we can hand it over to the
  // regular HPSDR protocol handling code
  radio->protocol = ORIGINAL_PROTOCOL;
  return 0;
}

void stemlab_cleanup(void) {
  if (curl_initialised) {
    curl_global_cleanup();
  }
}

#ifndef NO_AVAHI
//
// This is the avahi-dependent version of stemlab_discovery()
//
void stemlab_discovery(void) {
  discovery_done = FALSE;
  GaClient * const avahi_client = ga_client_new(GA_CLIENT_FLAG_NO_FLAGS);
  if (avahi_client == NULL) {
    fprintf(stderr, ERROR_PREFIX "Failed creating Avahi client\n");
    return;
  }
  GaServiceBrowser * const avahi_browser = ga_service_browser_new("_http._tcp");
  if (avahi_browser == NULL) {
    fprintf(stderr, ERROR_PREFIX "Failed creating Avahi browser\n");
    return;
  }
  GError *avahi_error = NULL;
  if (!ga_client_start(avahi_client, &avahi_error)) {
    fprintf(stderr, ERROR_PREFIX "Failed to start Avahi client: %s\n",
      avahi_error == NULL ? "(Unknown Error)" : avahi_error->message);
    return;
  }
  if (!ga_service_browser_attach(avahi_browser, avahi_client, &avahi_error)) {
    fprintf(stderr, ERROR_PREFIX "Failed attaching Avahi browser to client: %s\n",
      avahi_error == NULL ? "(Unknown Error)" : avahi_error->message);
    return;
  }
  const gulong new_service_handler =
    g_signal_connect(avahi_browser, "new-service", G_CALLBACK(new_service_cb),
        (gpointer) avahi_client);
  if (new_service_handler <= 0) {
    fprintf(stderr, ERROR_PREFIX "Failed installing browser \"new-service\" callback\n");
    return;
  }
  if(g_signal_connect(avahi_browser, "cache-exhausted",
      G_CALLBACK(cache_exhausted_cb), (gpointer) NULL) <= 0) {
    fprintf(stderr, ERROR_PREFIX "Failed installing browser \"cache-exhausted\" callback\n");
    g_signal_handler_disconnect(avahi_browser, new_service_handler);
    return;
  }
  // We need neither SSL nor Win32 sockets
  const CURLcode curl_error = curl_global_init(CURL_GLOBAL_NOTHING);
  if (curl_error != CURLE_OK) {
    fprintf(stderr, ERROR_PREFIX "Failed to initialise cURL: %s\n",
        curl_easy_strerror(curl_error));
    return;
  }
  curl_initialised = TRUE;
  while (!discovery_done || pending_callbacks > 0) {
    g_main_context_iteration(NULL, TRUE);
  }
}
#else
//
// This version of stemlab_discovery() needs libcurl
// but does not need avahi.
//
// Therefore we try to find the SDR apps on the RedPitaya
// assuming is has the (fixed) ip address which can be
// read from $HOME/.rp.inet, if this does not succeed it
// defaults to 192.168.1.3.
//
// So, on MacOS, just configure your STEMLAB/HAMLAB to this
// fixed IP address and you need not open a browser to start
// SDR *before* you can use piHPSDR.
//
// After starting the app in the main discover menu, we
// have to re-discover to get full info and start the radio.
//


void stemlab_discovery() {
  size_t len;
  char inet[20];
  char txt[150];
  CURL *curl_handle;
  CURLcode curl_error;
  FILE *fpin;
  char *p;
  int i;
  int app_list;
  struct sockaddr_in ip_address;
  struct sockaddr_in netmask;

  fprintf(stderr,"Stripped-down STEMLAB/HAMLAB discovery...\n");
//
// Try to read inet addr from $HOME/.rp.inet, otherwise take 192.168.1.3
//
   strcpy(inet,"192.168.1.3");
   p=getenv("HOME");
   if (p) {
     strncpy(txt,p, (size_t) 100);   // way less than size of txt
   } else {
     strcpy(txt,".");
   }
   strcat(txt,"/.rp.inet");
   fprintf(stderr,"Trying to read inet addr from file=%s\n", txt);
   fpin=fopen(txt, "r");
   if (fpin) {
     len=100;
     p=txt;
     len=getline(&p, &len, fpin);
     // not txt now contains the trailing newline character
     while (*p != 0) {
       if (*p == '\n') *p = 0;
       p++;
     }
     if (len < 20) strcpy(inet,txt);
   }
   fclose(fpin);
   fprintf(stderr,"STEMLAB: using inet addr %s\n", inet);
   ip_address.sin_family = AF_INET;
   inet_aton(inet, &ip_address.sin_addr);

   netmask.sin_family = AF_INET;
   inet_aton("0.0.0.0", &netmask.sin_addr);


//
// Do a HEAD request (poor curl's ping) to see whether the device is on-line
// allow a 15 sec time-out
//
  curl_handle = curl_easy_init();
  if (curl_handle == NULL) {
    fprintf(stderr, "stemlab_start: Failed to create cURL handle\n");
    return;
  }
  sprintf(txt,"http://%s",inet);
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_URL, txt);
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_NOBODY, (long) 1);
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, (long) 15);
  curl_error = curl_easy_perform(curl_handle);
  curl_easy_cleanup(curl_handle);
  if (curl_error ==  CURLE_OPERATION_TIMEDOUT) {
    sprintf(txt,"No response from web server at %s", inet);
    status_text(txt);
    fprintf(stderr,"%s\n",txt);
  }
  if (curl_error != CURLE_OK) {
    fprintf(stderr, "STEMLAB ping error: %s\n", curl_easy_strerror(curl_error));
    return;
  }
  
//
// Determine which SDR apps are present on the RedPitaya. The list may be empty.
//
  curl_handle = curl_easy_init();
  if (curl_handle == NULL) {
    fprintf(stderr, "stemlab_start: Failed to create cURL handle\n");
    return;
  }
  app_list=0;
  sprintf(txt,"http://%s/bazaar?apps=", inet);
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_URL, txt);
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, (long) 60);
  curl_error = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, app_list_cb);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &app_list);
  curl_error = curl_easy_perform(curl_handle);
  curl_easy_cleanup(curl_handle);
  if (curl_error == CURLE_OPERATION_TIMEDOUT) {
    status_text("No Response from RedPitaya in 60 secs");
    fprintf(stderr,"60-sec TimeOut met when trying to get list of HPSDR apps from RedPitaya\n");
  }
  if (curl_error != CURLE_OK) {
    fprintf(stderr, "STEMLAB app-list error: %s\n", curl_easy_strerror(curl_error));
    return;
  }
    
//
// Constructe "device" descripter. Hi-Jack the software version to
// encode which apps are present.
// What is needed in the interface data is only info.network.address.sin_addr,
// but the address and netmask of the interface must be compatible with this
// address to avoid an error condition upstream. That means
// (addr & mask) == (interface_addr & mask) must be fulfilled. This is easily
// achieved by setting interface_addr = addr and mask = 0.
//
  DISCOVERED *device = &discovered[devices++];
  device->protocol = STEMLAB_PROTOCOL;
  device->device = DEVICE_METIS;					// not used
  strcpy(device->name, "STEMlab");
  device->software_version = app_list;					// encodes list of SDR apps present
  device->status = STATE_AVAILABLE;
  memset(device->info.network.mac_address, 0, 6);			// not used
  device->info.network.address_length = sizeof(struct sockaddr_in);
  device->info.network.address.sin_family = AF_INET;
  device->info.network.address.sin_addr = ip_address.sin_addr;
  device->info.network.address.sin_port = htons(1024);
  device->info.network.interface_length = sizeof(struct sockaddr_in);
  device->info.network.interface_address = ip_address;			// same as RP address
  device->info.network.interface_netmask= netmask;			// does not matter
  strcpy(device->info.network.interface_name, "");
}

#endif
