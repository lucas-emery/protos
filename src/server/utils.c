#include "utils.h"

uint8_t to_lower(uint8_t in) {
    return in >= 'A' && in <= 'Z' ? (uint8_t)(in - 'A' + 'a') : in;
}

char* strcasestr( const char* str1, const char* str2 ) {
    const char* p1 = str1;
    const char* p2 = str2;
    const char* r = *p2 == 0 ? str1 : 0;

    while( *p1 != 0 && *p2 != 0 ) {

        if( to_lower( (unsigned char)*p1 ) == to_lower( (unsigned char)*p2 ) ) {
            if( r == 0 ) {
                r = p1;
            }
            p2++;
        } else {
            p2 = str2;
            if( r != 0 ) {
                p1 = r + 1;
            }

            if( to_lower( (unsigned char)*p1 ) == to_lower( (unsigned char)*p2 ) ) {
                r = p1;
                p2++;
            } else {
                r = 0;
            }
        }
        p1++;
    }

    return *p2 == 0 ? (char*)r : 0;
}