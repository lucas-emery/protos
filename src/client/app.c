#include "app.h"

int main(int argc, char const *argv[]) {
	int cantParams = argc-1;
	int sock, in, ret, port;
	struct sockaddr_in servaddr;
	char buffer[MAX_BUFFER], bufferAux[MAX_BUFFER], bufferRta[MAX_BUFFER], password[PASSWORD_SIZE + 1], ip[100];

	bzero(bufferAux, MAX_BUFFER);
	if(getParams(argc, argv, bufferAux, ip, &port) == 1) {
		return 1;
	}

	printf("\n");
	getPassword(password);
	printf("\n");

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	 
	if (sock < 0)
		DieWithSystemMessage("socket() failed");

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(ip);

	ret = connect(sock, (struct sockaddr *) &servaddr, sizeof (servaddr));

	if (ret < 0)
	  DieWithSystemMessage("connect() failed");

	bzero(buffer, MAX_BUFFER);
	strcpy(buffer, password);
	strcat(buffer, bufferAux);
	size_t datalen = strlen(buffer);
	ret = send(sock, (void *) buffer, datalen, 0);

	if(ret < 0 )
		DieWithSystemMessage("send() failed");
	else if (ret != datalen)
    		DieWithUserMessage("send()", "sent unexpected number of bytes");
	else
	{
		bzero(bufferRta, MAX_BUFFER);
		in = recv(sock, bufferRta, MAX_BUFFER, 0);
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

int getParams(int cantParams, char const *params[], char buffer[], char * ip, int * port) {
	int pos = 0;
	int ip1, ip2, ip3, ip4;
	int metrics[7] = {0};
	int configurations[4] = {0};
	char type;

	if(sscanf(params[1], "%d.%d.%d.%d:%d", &ip1, &ip2, &ip3, &ip4, port) == 5) {
		sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
	} else {
		printf("Invalid argument\n");
		return 1;
	}

	for(int i=2; i<cantParams; i++) {
		if(strlen(params[i]) == 3) {
			if(params[i][0] == '-') {
				switch(params[i][1]) {
					case 'm':
						type = params[i][2];
						switch(type) {
							case '1':
							case '2':
							case '3':
							case '4':
							case '5':
							case '6':
							case '7':
								if(!metrics[type-'0'-1]) {
									buffer[pos++] = '0';
									buffer[pos++] = type;
									metrics[type-'0'-1] = 1;
								} else {
									printf("Repeated argument\n");
									return 1;
								}
							break;
							default:
								printf("Invalid argument\n");
								return 1;
						}
					break;
					case 'c':
						type = params[i][2];
						switch(type) {
							case '0':
							case '1':
							case '2':
							case '3':
								if(i < cantParams-1) {
									i++;
									if(isMediaType(params[i])) {
										if(!configurations[type-'0']) {
											buffer[pos++] = '1';
											buffer[pos++] = type;
											buffer[pos] = 0;
											if(pos + strlen(params[i]) >= MAX_BUFFER-PASSWORD_SIZE) {
												printf("Media type is too long\n");
												return 1;
											}
											strcat(buffer, params[i]);
											pos += strlen(params[i]);
											buffer[pos++] = ' ';
											configurations[type-'0'] = 1;
										} else {
											printf("Repeated argument\n");
											return 1;
										}
									} else {
										printf("Invalid media type\n");
										return 1;
									}
								} else {
									printf("Invalid argument\n");
									return 1;
								}
							break;
							default:
								printf("Invalid argument\n");
								return 1;
						}
					break;
					default:
						printf("Invalid argument\n");
						return 1;
				}
			}
			else {
				printf("Invalid argument\n");
				return 1;
			}
		} 
		else {
			printf("Invalid argument\n");
			return 1;
		}
	}
	return 0;
}

int isMediaType(const char * param) {
	int j = 0;
	int len = strlen(param);
	char c;
	while(j < len) {
		c = param[j];
		if(c >= 'a' && c<= 'z') {
			j++;
		} else if(param[j] == '/') {
			j++;
			break;
		} else {
			return 0;
		}
	}
	if(j < len) {
		while(j < len) {
			c = param[j];
			if(c >= 'a' && c<= 'z') {
				j++;
			} else {
				return 0;
			}
		}
	} else {
		return 0;
	}
	return 1;
}

void parseResponse(char * buffer, int requests) {
	int j = 0;
	for(int i=0; i<requests; i++) {
		switch(buffer[j++]) {
			case '0':
				j++;
				for(int k=0; k<METRIC_SIZE; k++) //4 es la cant de bytes de una metricResponse
					printf("%c", buffer[j++]);
				printf("\n");
			break;
			case '1':
				if(buffer[j] == '0') {
					printf("Configurations reseted\n");
				} else {
					printf("Configuration %c applied\n", buffer[j++]);
				}	
			break;
			case '2':
				printf("Error in metric %c\n", buffer[j++]);
			break;
		}
	}
}

void getPassword(char * password) {
	int pass = 0, passlen;
	char a;

	while(!pass) {
		printf("Insert the password: ");
		bzero(password, PASSWORD_SIZE + 1);
		passlen = 0;
		while((a = getchar()) != '\n') {
			if(passlen >= PASSWORD_SIZE) {
				printf("The number of characters for password is %d\n", PASSWORD_SIZE);
				pass = 0;
				while(getchar() != '\n');
				break;
			}
			password[passlen++] = a;
			pass = 1;
		}
		if(pass == 1 && passlen < PASSWORD_SIZE) {
			printf("The number of characters for password is %d\n", PASSWORD_SIZE);
			pass = 0;
		}
	}
	password[passlen] = 0;
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

char * hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    int n = snprintf(NULL, 0, "%lu", hash);
	char * buf = malloc(n + 1);
	snprintf(buf, n+1, "%lu", hash);

    return buf;
}