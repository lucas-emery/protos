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
