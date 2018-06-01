#include "serverSCTP.h"

int main (int argc, char const *argv[])
{
  int listenSock, connSock, ret, in, flags, i;
  struct sockaddr_in servaddr;
  struct sctp_initmsg initmsg;
  struct sctp_event_subscribe events;
  struct sctp_sndrcvinfo sndrcvinfo;
  char buffer[MAX_BUFFER + 1];
 
  listenSock = socket (AF_INET, SOCK_STREAM, IPPROTO_SCTP);
  if(listenSock < 0)
    DieWithSystemMessage("socket() failed");
 
  memset(&servaddr, 0, sizeof(servaddr)); 
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl (INADDR_ANY);
  servaddr.sin_port = htons (MY_PORT_NUM);
 
  ret = bind (listenSock, (struct sockaddr *) &servaddr, sizeof (servaddr));
 
  if(ret < 0 )
    DieWithSystemMessage("bind() failed");
 
  /* Maximum of 5 streams will be available per socket */
  memset (&initmsg, 0, sizeof (initmsg));
  initmsg.sinit_num_ostreams = 5;
  initmsg.sinit_max_instreams = 5;
  initmsg.sinit_max_attempts = 4;
  ret = setsockopt (listenSock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof (initmsg));
 
  if(ret < 0 )
    DieWithSystemMessage("setsockopt() failed");
 
  ret = listen (listenSock, 5);

  if(ret < 0 )
    DieWithSystemMessage("listen() failed");
 
  while (1) {
    char buffer[MAX_BUFFER + 1], bufferRta[MAX_BUFFER + 1];
    size_t len;

    bzero (buffer, MAX_BUFFER + 1);

    printf ("Awaiting a new connection\n");

    struct sockaddr_in clntAddr;
    socklen_t clntAddrLen = sizeof(clntAddr);

    connSock = accept (listenSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
    if (connSock == -1)
      DieWithSystemMessage("accept() failed");
    else
      printf ("New client connected....\n");

    in = sctp_recvmsg (connSock, buffer, sizeof (buffer), (struct sockaddr *) NULL, 0, &sndrcvinfo, &flags);
    if( in < 0 )
      DieWithSystemMessage("sctp_recvmsg() failed");
    else 
    {
      printf("Successfully recieved %d bytes data to server\n", in);
      bzero(bufferRta, MAX_BUFFER + 1);
      parseRequest(buffer, bufferRta);
      len = strlen(bufferRta);
      ret = sctp_sendmsg(connSock, (void *) bufferRta, len, NULL, 0, 0, 0, 0, 0, 0);

      if(ret < 0 )
        DieWithSystemMessage("sctp_sendmsg() failed");
      else
        printf("Successfully sent %d bytes data to server\n", ret);  
    }
    close (connSock);
  }
  return 0;
}

void parseRequest(char * buffer, char * bufferRta) {
  char metric[4];
  char type;
  bzero(metric, strlen(metric));
  int cantRequests = strlen(buffer) / 2, bufferPos = 0, bufferRtaPos = 0;
  for(int i=0; i<cantRequests; i++) {
    switch(buffer[bufferPos++]) {
      case '0':
        type = buffer[bufferPos];
        getMetric(buffer[bufferPos++], metric);
        if(metric != 0) {
          bufferRta[bufferRtaPos++] = '0';
          bufferRta[bufferRtaPos++] = type;
          strcat(bufferRta, metric);
          bufferRtaPos = strlen(bufferRta);
        }
        else
          bufferRta[bufferRtaPos++] = '1';
      break;
      case '1':
        if(applyFilter(buffer[bufferPos++])) {
          bufferRta[bufferRtaPos++] = '2';
          int aux = bufferPos - 1;
          bufferRta[bufferRtaPos++] = buffer[aux];
        }
        else
          bufferRta[bufferRtaPos++] = '3';
      break;
    }
  }
}

void getMetric(char type, char * metric) {
  switch(type) {
    case '1':
      strcpy(metric, "0010");
    break;
    case '2':
      strcpy(metric, "0020");
    break;
    case '3':
      strcpy(metric, "0030");
    break;
  }
}

int applyFilter(char type) {
  return 1;
}

void DieWithUserMessage(const char *msg, const char *detail) {
  fputs(msg, stderr);
  fputs(": ", stderr);
  fputs(detail, stderr);
  fputc('\n', stderr);
  exit(1);
}

void DieWithSystemMessage(const char *msg) {
  perror(msg);
  exit(1);
}