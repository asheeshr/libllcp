/*
 * $Id$
 */

#include "config.h"

#include <stdio.h>
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
FILE *info_stream=NULL;
FILE *ndef_stream=NULL;

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
/////// put started
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
///////put ended


///////get started
static void *
g_send_first_packet(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t frame[107];
  
  uint8_t buffer[MAX_PACKET_LENGTH + 1];
  uint8_t l=0;
  frame[0]=0x10;
  frame[1]=0x01;
  frame[2]=frame[3]=frame[4]=0;
   frame[5]=/*sz*/9;
  frame[6]=frame[7]=frame[8]=0;
  frame[9]=200;
  frame[10]=0xD8;
  frame[11]=frame[12]=frame[13]=frame[14]=0;
  llc_connection_send(connection, frame, sizeof(frame)); //Frame sent
  printf("\nsent\n");
  return NULL;
}

static int
g_receive_first_packet(void * arg)
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
    int size=buf[5];
  char buffer[100];
  int l=0;
  while(buf[l+6]!='\0')
    {
      buffer[l]=buf[l+6];
      l++;
    }
  printf("%s",buffer);
  return size;
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

static void*
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
      fprintf(info_stream, "NDEF message received (%u bytes): %s\n", ndef_length,ndef_msg);
  }

  return NULL;
}

static void *
get_request(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  int len;

  //Receive First Frame
  printf("\nsending first packet");
  g_send_first_packet(connection);
  printf("\nreceiving first frame");
  len=g_receive_first_packet(connection);
  //Send Success / Continue
  if(len>=MAX_PACKET_LENGTH)
  {
    printf("\n sendinf continue");
      send_continue_packet(connection);
      //Receive Rest frames
      printf("receiving remaining data");
      receive_data(connection, len);
  }
  llc_connection_stop(connection);
  return NULL;
}



//////get ended


FILE *fp=NULL;

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


//  llc_connection_stop(connection);

  llc_connection_wait(con, NULL);
// closing connection
  llc_link_deactivate(llc_link);

  mac_link_free(mac_link);
  llc_link_free(llc_link);

  nfc_close(device);
  device = NULL;

  llcp_fini();
  nfc_exit(context);
  exit(EXIT_SUCCESS);
}
