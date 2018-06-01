#include <stdio.h>
#include <ctype.h>
#define BUFFERSIZE 1024

int main(int argc) {

    	char str[BUFFERSIZE];
	int r = 1;
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
    int count = 0;
    char c;
    do {
        c = getchar();

        if(c == '\n') {
            if((c = getchar()) == '\r')
                count++;
            if(count == 1)
                putchar(toupper(c));
            else
                putchar(c);
        }

        if(count == 1)
            putchar(toupper(c));
        else
            putchar(c);
    } while(count <= 1);
	 return 0;
}
