#include <stdio.h>
#include <ctype.h>
#define BUFFERSIZE 1024

int main(int argc) {

    	int count = 0;
    	char str[BUFFERSIZE];
	int r = 1;
	FILE * fp = fopen("./log.txt", "w+");
	/*
    	while(r) {
		r = read(0, str,  BUFFERSIZE);
		int i = 0;

		while(i < r){
        		str[i] = toupper(str[i]);
        		i++;
		}

		int n = 0;
		while(n < r){
			n += write(1, str, r - n);
		}
	 }
	*/
	char c;
	while((c = getchar()) != EOF){
		char * ptr = &c;
		fwrite(ptr, 1, 1, fp);
		if(c >= 'a' && c<='z')
			c += 'A' - 'a';
		putchar(c);
	}
	 return 0;
}
