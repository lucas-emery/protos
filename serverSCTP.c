//SCTPServer.C To compile - gcc sctpserver.c - o server - lsctp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#define MAX_BUFFER 1024
#define MY_PORT_NUM 9090 /* This can be changed to suit the need and should be same in server and client */
 
void parseRequest(char * buffer, char * bufferRta);
char getMetric(char type);
int applyFilter(char type);

int main (int argc, char const *argv[])
{
  int listenSock, connSock, ret, in, flags, i;
  struct sockaddr_in servaddr;
  struct sctp_initmsg initmsg;
  struct sctp_event_subscribe events;
  struct sctp_sndrcvinfo sndrcvinfo;
  char buffer[MAX_BUFFER + 1];
 
  listenSock = socket (AF_INET, SOCK_STREAM, IPPROTO_SCTP);
  if(listenSock == -1)
  {
      printf("Failed to create socket\n");
      perror("socket()");
      exit(1);
  }
 
  bzero ((void *) &servaddr, sizeof (servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl (INADDR_ANY);
  servaddr.sin_port = htons (MY_PORT_NUM);
 
  ret = bind (listenSock, (struct sockaddr *) &servaddr, sizeof (servaddr));
 
  if(ret == -1 )
  {
      printf("Bind failed \n");
      perror("bind()");
      close(listenSock);
      exit(1);
  }
 
  /* Specify that a maximum of 5 streams will be available per socket */
  memset (&initmsg, 0, sizeof (initmsg));
  initmsg.sinit_num_ostreams = 5;
  initmsg.sinit_max_instreams = 5;
  initmsg.sinit_max_attempts = 4;
  ret = setsockopt (listenSock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof (initmsg));
 
  if(ret == -1 )
  {
      printf("setsockopt() failed \n");
      perror("setsockopt()");
      close(listenSock);
      exit(1);
  }
 
  ret = listen (listenSock, 5);
  if(ret == -1 )
  {
      printf("listen() failed \n");
      perror("listen()");
      close(listenSock);
      exit(1);
  }
 
  while (1) {
    char buffer[MAX_BUFFER + 1], bufferRta[MAX_BUFFER + 1];
    int len;

    //Clear the buffer
    bzero (buffer, MAX_BUFFER + 1);

    printf ("Awaiting a new connection\n");

    connSock = accept (listenSock, (struct sockaddr *) NULL, (int *) NULL);
    if (connSock == -1)
    {
      printf("accept() failed\n");
      perror("accept()");
      close(connSock);
      continue;
    }
    else
      printf ("New client connected....\n");

    in = sctp_recvmsg (connSock, buffer, sizeof (buffer), (struct sockaddr *) NULL, 0, &sndrcvinfo, &flags);
    if( in == -1)
    {
        printf("Error in sctp_recvmsg\n");
        perror("sctp_recvmsg()");
        close(connSock);
        continue;
    }
    else 
    {
      //Add '\0' in case of text data
      //buffer[in] = '\0';
      //printf (" Length of Data received: %d\n", in);
      //printf (" Data : %s\n", (char *) buffer);

      bzero(bufferRta, MAX_BUFFER + 1);
      parseRequest(buffer, bufferRta);
      len = strlen(bufferRta);
      printf("AAA\n");
      /*ret = sctp_sendmsg(connSock, (void *) bufferRta, (size_t) len, NULL, 0, 0, 0, 0, 0, 0);
      printf("AAA\n");

      if(ret == -1 )
      {
        printf("Error in sctp_sendmsg\n");
        perror("sctp_sendmsg()");
        continue;
      }
      else
        printf("Successfully sent %d bytes data to server\n", ret);
        */     
    }
      close (connSock);
  }
  return 0;
}

void parseRequest(char * buffer, char * bufferRta) {
  char metric;
  switch(buffer[0]) {
    case '0':
      metric = getMetric(buffer[1]);
      if(metric != 0) {
        bufferRta[0] = '0';
        bufferRta[1] = metric;
      }
      else
        bufferRta[0] = '1';
    break;
    case '1':
      if(applyFilter(buffer[1])) {
        bufferRta[0] = '2';
        bufferRta[1] = buffer[1];
      }
      else
        bufferRta[0] = '3';  
    break;
  }
}

char getMetric(char type) {
  return type;
}

int applyFilter(char type) {
  return 1;
}