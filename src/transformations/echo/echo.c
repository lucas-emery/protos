#include <stdio.h>
#include <ctype.h>

int main(int argc, char const *argv[]) {
    int count = 0;
    char c;
    do {
        c = getchar();

        if(c == '\n') {
            if((c = getchar()) == '\r')
                count++;
            putchar(c);
        }

        putchar(c);
    } while(count <= 1);
    return 0;
}
