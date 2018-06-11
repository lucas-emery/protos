#include <stdio.h>

int main(){
	FILE * fp = fopen("log.txt", "w");
	char c;
	while((c = getchar()) != EOF){
		putchar(c);
		fputc(c, fp);
	}
	fclose(fp);
	return 0;
}
