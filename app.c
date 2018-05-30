#include "app.h"

int main(int argc, char const *argv[]) {
	int cantParams = argc-1;
	/*
	struct chunk chunks[6];
	*/
	int connSock, in, i, ret, flags;
	struct sockaddr_in servaddr;
	struct sctp_status status;
	struct sctp_sndrcvinfo sndrcvinfo;
	char buffer[MAX_BUFFER + 1], bufferRta[MAX_BUFFER + 1];
	int datalen = 0;

	if(getParams(argc, argv, buffer) == 1) {
		printf("Error de parametro\n");
		return 1;
	}
	datalen = strlen(buffer);
	/*
	buffer[0] = chunks[0].chunkValueType;
	buffer[1] = chunks[0].chunkValueSpecification;
	buffer[2] = 0;
	buffer[3] = 0;
	datalen = 2;
	*/

	connSock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	 
	if (connSock == -1)
	{
	  printf("Socket creation failed\n");
	  perror("socket()");
	  exit(1);
	}

	bzero((void *) &servaddr, sizeof (servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(MY_PORT_NUM);
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	ret = connect(connSock, (struct sockaddr *) &servaddr, sizeof (servaddr));

	if (ret == -1)
	{
	  printf("Connection failed\n");
	  perror("connect()");
	  close(connSock);
	  exit(1);
	}

	ret = sctp_sendmsg(connSock, (void *) buffer, (size_t) datalen, NULL, 0, 0, 0, 0, 0, 0);

	if(ret == -1 )
	{
		printf("Error in sctp_sendmsg\n");
		perror("sctp_sendmsg()");
	}
	else
	{
		printf("Successfully sent %d bytes data to server\n", ret);

		bzero(bufferRta, MAX_BUFFER + 1);
		in = sctp_recvmsg (connSock, bufferRta, sizeof (bufferRta), (struct sockaddr *) NULL, 0, &sndrcvinfo, &flags);
		if( in == -1)
    	{
	        printf("Error in sctp_recvmsg\n");
	        perror("sctp_recvmsg()");
	        close(connSock);
	    }
	    else 
    	{
    		parseResponse(bufferRta);
    	}
	}

	close (connSock);

	return 0;	
}
/*
struct chunk generateChunk(uint8_t valueType, uint8_t valueSpecification) {
	struct chunk c;
	c.chunkValueType = valueType;
	c.chunkValueSpecification = valueSpecification;

	return c;
}
*/

int getParams(int cantParams, char const *params[], char buffer[]) {
	for(int i=1; i<cantParams; i++) {
		if(strlen(params[i]) == 3) {
			if(params[i][0] == '-') {
				switch(params[i][1]) {
					case 'm':
						switch(params[i][2]) {
							case '1':
								buffer[0] = '0';
								buffer[1] = '1';

							break;
							case '2':
								buffer[0] = '0';
								buffer[1] = '2';
							break;
							case '3':
								buffer[0] = '0';
								buffer[1] = '3';
							break;
							default:
								return 1;
						}
					break;
					case 'c':
						switch(params[i][2]) {
							case '1':
								buffer[0] = '1';
								buffer[1] = '1';
							break;
							case '2':
								buffer[0] = '2';
								buffer[1] = '1';
							break;
							case '3':
								buffer[0] = '3';
								buffer[1] = '1';
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

void parseResponse(char * buffer) {
	switch(buffer[0]) {
		case '0':
			printf("Metrica %c\n", buffer[1]);
		break;	
		case '1':
			printf("Error de metrica\n");
		break;
		case '2':
			printf("Se ha aplicado el filtro %c\n", buffer[1]);
		break;
		case '3':
			printf("Error aplicando el filtro\n");
		break;
	}
}