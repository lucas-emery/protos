#include "app.h"

int main(int argc, char const *argv[]) {
	int cantParams = argc-1;
	int sock, in, i, ret, flags;
	struct sockaddr_in servaddr;
	struct sctp_status status;
	struct sctp_sndrcvinfo sndrcvinfo;
	char buffer[MAX_BUFFER + 1], bufferRta[MAX_BUFFER + 1];

	bzero(buffer, MAX_BUFFER + 1);
	if(getParams(argc, argv, buffer) == 1) {
		printf("Error de parametro\n");
		return 1;
	}

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	 
	if (sock < 0)
		DieWithSystemMessage("socket() failed");

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(MY_PORT_NUM);
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	ret = connect(sock, (struct sockaddr *) &servaddr, sizeof (servaddr));

	if (ret < 0)
	  DieWithSystemMessage("connect() failed");

	size_t datalen = strlen(buffer);
	ret = sctp_sendmsg(sock, (void *) buffer, datalen, NULL, 0, 0, 0, 0, 0, 0);

	if(ret < 0 )
		DieWithSystemMessage("send() failed");
	else if (ret != datalen)
    	DieWithUserMessage("send()", "sent unexpected number of bytes");
	else
	{
		bzero(bufferRta, MAX_BUFFER + 1);
		in = sctp_recvmsg (sock, bufferRta, sizeof (bufferRta), (struct sockaddr *) NULL, 0, &sndrcvinfo, &flags);
		if( in < 0 )
	        DieWithSystemMessage("recv() failed");
	    else if (in == 0)
      		DieWithUserMessage("recv()", "connection closed prematurely");
	    else 
    	{
    		parseResponse(bufferRta, cantParams);
    	}
	}

	close (sock);
	return 0;	
}

int getParams(int cantParams, char const *params[], char buffer[]) {
	int pos = 0;
	for(int i=1; i<cantParams; i++) {
		if(strlen(params[i]) == 3) {
			if(params[i][0] == '-') {
				switch(params[i][1]) {
					case 'm':
						switch(params[i][2]) {
							case '1':
								buffer[pos++] = '0';
								buffer[pos++] = '1';

							break;
							case '2':
								buffer[pos++] = '0';
								buffer[pos++] = '2';
							break;
							case '3':
								buffer[pos++] = '0';
								buffer[pos++] = '3';
							break;
							default:
								return 1;
						}
					break;
					case 'c':
						switch(params[i][2]) {
							case '1':
								buffer[pos++] = '1';
								buffer[pos++] = '1';
							break;
							case '2':
								buffer[pos++] = '1';
								buffer[pos++] = '2';
							break;
							case '3':
								buffer[pos++] = '1';
								buffer[pos++] = '3';
							break;
							default:
								return 1;
						}
					break;
					default:
						return 1;
				}
			}
			else {
				return 1;
			}
		} 
		else {
			return 1;
		}
	}
	return 0;
}

void parseResponse(char * buffer, int requests) {
	int j = 0;
	for(int i=0; i<requests; i++) {
		switch(buffer[j++]) {
			case '0':
				printf("Metrica %c: ", buffer[j++]);
				for(int k=0; k<4; k++) //4 es la cant de bytes de una metricResponse
					printf("%c", buffer[j++]);
				printf("\n");
			break;	
			case '1':
				printf("Error en la metrica %c\n", buffer[j++]);
			break;
			case '2':
				printf("Se ha aplicado el filtro %c\n", buffer[j++]);
			break;
			case '3':
				printf("Error aplicando el filtro %c\n", buffer[j++]);
			break;
		}
	}
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