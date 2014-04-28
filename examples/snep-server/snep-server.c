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

int parameters;
char **sequence;

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
FILE *fp;
///////// put response
static int
receive_first_frame(void *arg, char filename[])
{
    struct llc_connection *connection = (struct llc_connection *) arg;
    uint8_t buffer[MAX_PACKET_LENGTH+7];
//    int ndef_length;
    int len;
    FILE * output;
    output = fopen(filename, "w");
    
    if ((len = llc_connection_recv(connection, buffer,/* ((sizeof(buffer)-6)<buffer[5])?(sizeof(buffer)-6):buffer[5]*/sizeof(buffer), NULL)) < 0)
	return -1;
    
    if (len < 2) // SNEP's header (2 bytes)  and NDEF entry header (5 bytes)
	return -1;
    
    size_t n = 0;
    
    // Header
    //  fprintf(info_stream, "SNEP version: %d.%d\n", (buffer[0]>>4), (buffer[0]&0x0F));
    printf("SNEP version: %d.%d\n", (buffer[0]>>4), (buffer[0]&0x0F));
    if (buffer[n++] != 0x10){
	printf("snep-server is developed to support snep version 1.0, version %d.%d may be not supported.\n", (buffer[0]>>4), (buffer[0]&0x0F));
    }
    
//    char ndef_msg[101];
    printf("total data sizee : %d\n",buffer[5]);
    //ndef_length = buffer[5] < MAX_PACKET_LENGTH ? buffer[5] : MAX_PACKET_LENGTH ;
    //shexdump(ndef_msg, buffer + 6, ndef_length);
    //fprintf(info_stream, "NDEF message received (%u bytes): %s\n", ndef_length, ndef_msg);
    printf("\nOutput (Received %d, Remaining Frame data %d) : %s\n", len, buffer[5]-len, buffer+6);
    fprintf(output, "%s", buffer+6);
    
    fclose(output);

    return buffer[5];
}

static int
receive_data(void *arg, int len)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t buffer[MAX_PACKET_LENGTH+7];
  //int rec_len;
  //  int ndef_length;
  int data_len = len - (MAX_PACKET_LENGTH), l3;
  
  while(data_len>0)
  {
    l3 = llc_connection_recv(connection, buffer, ((sizeof(buffer)-6)>data_len+1)?data_len+1:sizeof(buffer), NULL);
//    buffer[(data_len>MAX_PACKET_LENGTH)?MAX_PACKET_LENGTH+6+1:data_len+6+1]='\0';
      buffer[l3+6+1]='\0';
      //size_t n = 0;
      //char ndef_msg[101];
      //shexdump(ndef_msg, buffer + 6, ndef_length);
      //fprintf(info_stream, "NDEF message received (%u bytes): %s\n", ndef_length, ndef_msg);
    printf("\nOutput (Received %d, Remaining Frame data %d) : %s\n", l3, data_len, buffer+6);
     data_len-=MAX_PACKET_LENGTH;
  }

  return data_len;
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
put_response(void *arg, char filename[])
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  int len, l2;

  //Receive First Frame
  len = receive_first_frame(connection, filename);
  
  //Send Success / Continue
  if(len>=MAX_PACKET_LENGTH)
  {
      send_continue_packet(connection);
      //Receive Rest frames
      l2 = receive_data(connection, len);
      printf("Data received\n");
  }
  
  if(l2 <= 0)
  {
      printf("Sending Success!\n");
      send_success_packet(connection);
      printf("Success!");
  }
  

  //llc_connection_stop(connection);
  return NULL;
}
//////put response ended

//////get response started

static long
g_send_first_packet(void *arg, char filename[])
{
    struct llc_connection *connection = (struct llc_connection *) arg;
    uint8_t frame[MAX_PACKET_LENGTH+7];
    uint8_t buffer[MAX_PACKET_LENGTH + 1];
    uint8_t l=0;
    FILE *input;
    input = fopen(filename, "r");
    
    if(input == NULL)
      {
	printf("File not found");
	exit(0);
      }
    
    fseek(input, 0L, SEEK_END);
    long sz = ftell(input);
    fseek(input, -(sz) ,SEEK_END);      
    frame[0]=0x10;
    frame[1]=0x00;
    frame[2]=frame[3]=frame[4]=0;
  
    fread(buffer,sizeof(char),MAX_PACKET_LENGTH,input);
    buffer[(sz>MAX_PACKET_LENGTH?MAX_PACKET_LENGTH:sz)] = '\0';
    printf("Buffer contains (%d) : %s\n", sz, buffer);

  // frame[5]=strlen(buffer);
    frame[5]=sz;

    while(buffer[l]!='\0')
	frame[l+6]=buffer[l++];
    frame[l+6]='\0';
    
    llc_connection_send(connection, frame, sizeof(frame)); //Frame sent
    printf("\nsent\n");

    //    fclose(input);

    return sz;
}

static void*
g_receive_first_packet(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
//>>>>>>> 12cb15b091ed052a29a4a26e2af6b7996516fa0d
  uint8_t buffer[MAX_PACKET_LENGTH];
  int ndef_length;

  int len;
  if ((len = llc_connection_recv(connection, buffer, sizeof(buffer), NULL)) < 0)
    return NULL;

  if (len < 2) // SNEP's header (2 bytes)  and NDEF entry header (5 bytes)
    return NULL;

  size_t n = 0;
  printf("\n length of buffer: %d\n",len);
  // Header
//  fprintf(info_stream, "SNEP version: %d.%d\n", (buffer[0]>>4), (buffer[0]&0x0F));
  if (buffer[n++] != 0x10){
    printf("snep-server is developed to support snep version 1.0, version %d.%d may be not supported.\n", (buffer[0]>>4), (buffer[0]&0x0F));
  }
  printf("\n end\n");
    char ndef_msg[101];
  //<<<<<<< HEAD
//  printf("\nreceived string: %s\n",buffer+6);
  //    shexdump(ndef_msg, buffer + 6, ndef_length);
  //  fprintf(info_stream, "NDEF message received (%u bytes): %s\n", ndef_length, ndef_msg);

  //  return buffer[5];
  return NULL;
}

static int
g_receive_continue(void * arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t buf[MAX_PACKET_LENGTH+7];
  int ret;
  uint8_t ssap;
  
//  ret = 
  llc_connection_recv(connection, buf, sizeof(buf), &ssap);//Continue or Success
  
//  llc_connection_stop(connection);
  if(buf[1]==0x80) //Continue
  {
      printf("continue");
      return 0x80;
  }

}



static void *
g_send_data(void *arg)
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t frame[MAX_PACKET_LENGTH];
  char buffer[MAX_PACKET_LENGTH + 1];
  uint8_t l=0;
      
//
  frame[0]=0x10;
  frame[1]=0x02;
  frame[2]=frame[3]=frame[4]=0;
//
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
get_response(void *arg, char filename[])
{
  struct llc_connection *connection = (struct llc_connection *) arg;
  uint8_t ret;
  int len;
  printf("Sending GET response\n");
  g_receive_first_packet(connection); //Sends GET request packet
  printf("Back in put_Response");
  len=g_send_first_packet(connection, filename);
  printf("Back in put_Response");
  fflush(stdout);

  if(len>MAX_PACKET_LENGTH)
    {
      g_receive_continue(connection);
      g_send_data(connection);
    }
  //  llc_connection_stop(connection);
  
  printf("returing to multi");
  return NULL;
}
//////get response ended

static void
print_usage(char *progname)
{
  fprintf(stderr, "usage: %s -o FILE\n", progname);
  fprintf(stderr, "\nOptions:\n");
  fprintf(stderr, "  -o     Extract NDEF message if available in FILE\n");
}

static void *
multi_protocol(void * arg)
{
    struct llc_connection *connection = (struct llc_connection *) arg;
    int index = 1;

#ifdef DEBUG_TIME
    struct timeval stop, start;
#endif
    
    while(index<parameters)
    {	
	if(sequence[index][0]=='g')
	{
	    
#ifdef DEBUG_TIME
	    gettimeofday(&start, NULL);
#endif
	    
	    printf("************GET*********************\n");
	    get_response(arg, sequence[index+1]);
	    printf("Back from get");;
	    //	    fseek(fp, 0, SEEK_SET);
	    
#ifdef DEBUG_TIME
	    gettimeofday(&stop, NULL);
	    printf("took %lu\n", stop.tv_usec - start.tv_usec);   
#endif
	}
	
	else if(sequence[index][0]=='p')
	{
	    
#ifdef DEBUG_TIME
	    gettimeofday(&start, NULL);
#endif
	    
	    printf("*************PUT********************\n");
	    put_response(arg, sequence[index+1]);
	    
#ifdef DEBUG_TIME
	    gettimeofday(&stop, NULL);
	    printf("took %lu\n", stop.tv_usec - start.tv_usec);   
#endif
	}

	index+=2;/* To skip a option and filename */
    }

    llc_connection_stop(connection);    
}


int
main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  //char filename[50];

  if(argc<3) /* argc should be 3 for correct execution */
  {
    printf( "usage: %s filename -[p/g] filename ...", argv[0] );
    return 0;
  }
  
  parameters = argc;
  sequence = argv;

  printf("The parameters received were:");
  for(int i=1; i<parameters; i++)
  {
      printf("%s \n", sequence[i]);
  }
//  return 0;
  

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
  /*if (!(*/com_android_snep = llc_service_new_with_uri(NULL, multi_protocol/*put_response/*com_android_snep_thread*/, "urn:nfc:sn:snep", NULL);/*))
    errx(EXIT_FAILURE, "Cannot create com.android.snep service");*/
  
  //  printf("First PUT completed");

  //  /*if (!(*/com_android_snep = llc_service_new_with_uri(NULL, put_response/*com_android_snep_thread*/, "urn:nfc:sn:snep", NULL);/*))
  //  errx(EXIT_FAILURE, "Cannot create com.android.snep service");*/

  /*  struct llc_service *com_android_snep;
      if (!(com_android_snep = llc_service_new_with_uri(NULL, get_response/*com_android_snep_thread*//*, "urn:nfc:sn:snep", NULL)))
errx(EXIT_FAILURE, "Cannot create com.android.snep service");*/
  
  llc_service_set_miu(com_android_snep, 512);
  llc_service_set_rw(com_android_snep, 2);
  
  //printf("Test1\n");
  
  if (llc_link_service_bind(llc_link, com_android_snep, LLCP_SNEP_SAP) < 0)
      errx(EXIT_FAILURE, "Cannot bind service");
  
  printf("Test2\n");
  //do{
  mac_link = mac_link_new(device, llc_link);
  if (!mac_link)
      errx(EXIT_FAILURE, "Cannot create MAC link");
  
  //printf("Test3\n");
  //fflush(stdout);
  
  if (mac_link_activate_as_target(mac_link) < 0) {
      errx(EXIT_FAILURE, "Cannot activate MAC link");
  }

  //printf("Test4\n");
  
  void *err;
  mac_link_wait(mac_link, &err);
  
  //printf("Test5\n");  
  //flag--;
  //  }while(flag);
  
  mac_link_free(mac_link);
  llc_link_free(llc_link);
  
  nfc_close(device);
  device = NULL;
  
  llcp_fini();
  nfc_exit(context);
  exit(EXIT_SUCCESS);
}
