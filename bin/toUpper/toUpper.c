#include <stdio.h>

int main(){
	char c;
	while((c = getchar()) != EOF){
		if(c <= 'z' && c >= 'a')
			c += 'A' - 'a';
		putchar(c);
	}
	return 0;
}
