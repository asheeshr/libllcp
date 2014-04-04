/*
 * $Id$
 */

#include "config.h"

#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <llcp.h>
#include <llc_service.h>
#include <llc_link.h>
#include <mac.h>
#include <llc_connection.h>

#define MAX_PACKET_LENGTH 100

struct mac_link *mac_link;
nfc_device *device;

static void
stop_mac_link(int sig)
{
  (void) sig;

  if (mac_link && mac_link->device)
    nfc_abort_command(mac_link->device);
}

static void
bye(void)
{
  if (device)
    nfc_close(device);
}

static size_t
shexdump(char *dest, const uint8_t *buf, const size_t size)
{
  size_t res = 0;
  for (size_t s = 0; s < size; s++) {
    sprintf(dest + res, "%02x  ", *(buf + s));
    res += 4;
  }
  return res;
}

FILE *info_stream = NULL;
FILE *ndef_stream = NULL;


int
receive_first_frame(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t buffer[107];
  int ndef_length;

  int len;
  if ((len = llc_connection_recv(connection, buffer, sizeof(buffer), NULL)) < 0)
    return NULL;

  if (len < 2) // SNEP's header (2 bytes)  and NDEF entry header (5 bytes)
    return NULL;

  size_t n = 0;

  // Header
  fprintf(info_stream, "SNEP version: %d.%d\n", (buffer[0]>>4), (buffer[0]&0x0F));
  if (buffer[n++] != 0x10){
    printf("snep-server is developed to support snep version 1.0, version %d.%d may be not supported.\n", (buffer[0]>>4), (buffer[0]&0x0F));
  }

  char ndef_msg[101];
  shexdump(ndef_msg, buffer + 6, ndef_length);
  fprintf(info_stream, "NDEF message received (%u bytes): %s\n", ndef_length, ndef_msg);

  return buffer[5];
}

int
receive_data(void *arg, int len)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t buffer[107];
  int rec_len;
  int ndef_length;
  int data_len = len - MAX_PACKET_LENGTH;
  
  while(data_len>0)
  {
      llc_connection_recv(connection, buffer, sizeof(buffer), NULL);
      data_len-=MAX_PACKET_LENGTH;
      size_t n = 0;
      char ndef_msg[101];
      shexdump(ndef_msg, buffer + 6, ndef_length);
      fprintf(info_stream, "NDEF message received (%u bytes): %s\n", ndef_length, ndef_msg);
  }

  return NULL;
}


static void *
send_continue_packet(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t frame[1024];
  
  /** return snep continue response package */
  frame[0] = 0x10;    /** SNEP version */
  frame[1] = 0x80;
  frame[2] = 0;
  frame[3] = 0;
  frame[4] = 0;
  frame[5] = 0;
  llc_connection_send(connection, frame, 6);

  return NULL;
}

static void *
send_success_packet(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t frame[1024];
  
  /** return snep success response package */
  frame[0] = 0x10;    /** SNEP version */
  frame[1] = 0x81;
  frame[2] = 0;
  frame[3] = 0;
  frame[4] = 0;
  frame[5] = 0;
  llc_connection_send(connection, frame, 6);

  return NULL;
}

static void *
put_response(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  int len;

  //Receive First Frame
  len = receive_first_frame(connection);
  
  //Send Success / Continue
  if(len>=MAX_PACKET_LENGTH)
  {
      send_continue_packet(connection);
      //Receive Rest frames
      receive_data(connection, len);
  }
  else
  {
      send_success_packet(connection);
      printf("Success!");
  }

  llc_connection_stop(connection);
  return NULL;
}


static void *
com_android_snep_service(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t frame[] = {
    0x10, 0x02,
    0x00, 0x00, 0x00, 33,
    0xd1, 0x02, 0x1c, 0x53, 0x70, 0x91, 0x01, 0x09, 0x54, 0x02,
    0x65, 0x6e, 0x4c, 0x69, 0x62, 0x6e, 0x66, 0x63, 0x51, 0x01,
    0x0b, 0x55, 0x03, 0x6c, 0x69, 0x62, 0x6e, 0x66, 0x63, 0x2e,
    0x6f, 0x72, 0x67
  };
  uint8_t buf[1024];
  int ret;
  uint8_t ssap;

  llc_connection_send(connection, frame, sizeof(frame));

  ret = llc_connection_recv(connection, buf, sizeof(buf), &ssap);
  if(ret>0){
    printf("Send NDEF message done.\n");
  }else if(ret ==  0){
    printf("Received no data\n");
  }else{
    printf("Error received data.");
  }
  llc_connection_stop(connection);

  return NULL;
}

static void
print_usage(char *progname)
{
  fprintf(stderr, "usage: %s -o FILE\n", progname);
  fprintf(stderr, "\nOptions:\n");
  fprintf(stderr, "  -o     Extract NDEF message if available in FILE\n");
}

int
main(int argc, char *argv[])
{
  int ch;
  char *ndef_output = NULL;

  while ((ch = getopt(argc, argv, "ho:")) != -1) {
    switch (ch) {
      case 'h':
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
        break;
      case 'o':
        ndef_output = optarg;
        break;
      case '?':
        if (optopt == 'o')
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      default:
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if (ndef_output == NULL) {
    // No output sets by user
    print_usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((strlen(ndef_output) == 1) && (ndef_output[0] == '-')) {
    info_stream = stderr;
    ndef_stream = stdout;
  } else {
    info_stream = stdout;
    ndef_stream = fopen(ndef_output, "wb");
    if (!ndef_stream) {
      fprintf(stderr, "Could not open file %s.\n", ndef_output);
      exit(EXIT_FAILURE);
    }
  }

  nfc_context *context;
  nfc_init(&context);

  if (llcp_init() < 0)
    errx(EXIT_FAILURE, "llcp_init()");

  signal(SIGINT, stop_mac_link);
  atexit(bye);

  if (!(device = nfc_open(context, NULL))) {
    errx(EXIT_FAILURE, "Cannot connect to NFC device");
  }

  struct llc_link *llc_link = llc_link_new();
  if (!llc_link) {
    errx(EXIT_FAILURE, "Cannot allocate LLC link data structures");
  }

  struct llc_service *com_android_snep;
  if (!(com_android_snep = llc_service_new_with_uri(NULL, put_response/*com_android_snep_thread*/, "urn:nfc:sn:snep", NULL)))
    errx(EXIT_FAILURE, "Cannot create com.android.snep service");

  llc_service_set_miu(com_android_snep, 512);
  llc_service_set_rw(com_android_snep, 2);

  if (llc_link_service_bind(llc_link, com_android_snep, LLCP_SNEP_SAP) < 0)
    errx(EXIT_FAILURE, "Cannot bind service");

  mac_link = mac_link_new(device, llc_link);
  if (!mac_link)
    errx(EXIT_FAILURE, "Cannot create MAC link");

  if (mac_link_activate_as_target(mac_link) < 0) {
    errx(EXIT_FAILURE, "Cannot activate MAC link");
  }

  void *err;
  mac_link_wait(mac_link, &err);

  mac_link_free(mac_link);
  llc_link_free(llc_link);

  nfc_close(device);
  device = NULL;

  llcp_fini();
  nfc_exit(context);
  exit(EXIT_SUCCESS);
}
