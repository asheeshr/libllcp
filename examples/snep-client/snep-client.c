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

FILE *fp;

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

static void printhex(char *s, uint8_t *buf, int len)
{
  printf("%s", s);
  int i;
  for(i=0; i<len; i++){
    printf("%02X ", buf[i]);
  }
  printf("\n");
}

static void
print_usage(char *progname)
{
  fprintf(stderr, "usage: %s\n", progname);
}

static void *
send_first_packet(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t frame[107];
  uint8_t buffer[MAX_PACKET_LENGTH + 1];
  uint8_t l=0;
  fseek(fp, 0L, SEEK_END);
  int sz = ftell(fp);
  fseek(fp, -(sz) ,SEEK_END);      
  frame[0]=0x10;
  frame[1]=0x02;
  frame[2]=frame[3]=frame[4]=0;
  
  fread(buffer,sizeof(char),MAX_PACKET_LENGTH,fp);
  printf("Buffer contains : %s\n", buffer);

  // frame[5]=strlen(buffer);
  frame[5]=sz;

  while(buffer[l]!='\0')
      frame[l+6]=buffer[l++];

  llc_connection_send(connection, frame, sizeof(frame)); //Frame sent
  printf("\nsent\n");
  return NULL;
}

static int
receive_packet(void * arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t buf[MAX_PACKET_LENGTH+7];
  int ret;
  uint8_t ssap;
  
//  ret = 
  llc_connection_recv(connection, buf, sizeof(buf), &ssap);//Continue or Success
  /* if(ret>0){
    printf("Send NDEF message done.\n");
  }else if(ret ==  0){
    printf("Received no data\n");
  }else{
    printf("Error received data.");
  }
  */
//  llc_connection_stop(connection);
  
  if(buf[1]==0x81) //Success
  {
      printf("\nsuccess");
      return 0x81;
  }
  else if(buf[1]==0x80) //Continue
  {
      printf("continue");
      return 0x80;
  }

}



static void *
send_data(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t frame[107];
  char buffer[MAX_PACKET_LENGTH + 1];
  uint8_t l=0;
      
///*
  frame[0]=0x10;
  frame[1]=0x02;
  frame[2]=frame[3]=frame[4]=0;
//*/
  while(feof(fp)==0) //Send remaining data
  {
      fread(buffer,sizeof(char),MAX_PACKET_LENGTH,fp);
      printf("Buffer contains : %s\n", buffer);
  
      frame[5]=strlen(buffer);
  
      while(buffer[l]!='\0')
	  frame[l+6]=buffer[l++];
      
      llc_connection_send(connection, frame, sizeof(frame)); //Frame sent
  }

  return NULL;
}

static void *
put_request(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t ret;
  
  printf("Sending PUT request\n");
  send_first_packet(connection); //Sends PUT request packet
  
  printf("Waiting for response\n");
  while((ret=receive_packet(connection))!=0x81)
  {
      printf("Sending data");
      send_data(connection);
  }
  
  if(ret==0x81)
  {
      printf("Success!");
  }
  else
  {
      printf("Error :%d", ret);
  }

  llc_connection_stop(connection);

  return NULL;
}



/*
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
  }*/






int
main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  char filename[50];
  
  printf("Enter file to transfer:");
  scanf("%s", filename);
  fp = fopen(filename, "r");
      

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

  mac_link = mac_link_new(device, llc_link);
  if (!mac_link){
    errx(EXIT_FAILURE, "Cannot create MAC link");
  }
  
  struct llc_service *com_android_npp;
  //------------------------

  if (!(com_android_npp = llc_service_new(NULL, put_request/*com_android_snep_service*/, NULL))){
    errx(EXIT_FAILURE, "Cannot create com.android.npp service");
  }
  //-------------------------
  
  llc_service_set_miu(com_android_npp, 512);
  llc_service_set_rw(com_android_npp, 2);

  int sap;
  if ((sap = llc_link_service_bind(llc_link, com_android_npp, 0x20)) < 0)
    errx(EXIT_FAILURE, "Cannot bind service");

//  struct llc_connection *con = llc_outgoing_data_link_connection_new_by_uri(llc_link, sap, "urn:nfc:sn:snep");
  struct llc_connection *con = llc_outgoing_data_link_connection_new(llc_link, sap, LLCP_SNEP_SAP);
  if (!con){
    errx(EXIT_FAILURE, "Cannot create llc_connection");
  }

  if (mac_link_activate_as_initiator(mac_link) < 0) {
    errx(EXIT_FAILURE, "Cannot activate MAC link");
  }

  if (llc_connection_connect(con) < 0)
    errx(EXIT_FAILURE, "Cannot connect llc_connection");

  llc_connection_wait(con, NULL);

  llc_link_deactivate(llc_link);

  mac_link_free(mac_link);
  llc_link_free(llc_link);

  nfc_close(device);
  device = NULL;

  llcp_fini();
  nfc_exit(context);
  exit(EXIT_SUCCESS);
}
