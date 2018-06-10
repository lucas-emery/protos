#ifndef PC_2018_07_TRANSFORMATION_chunk_H
#define PC_2018_07_TRANSFORMATION_chunk_H

#include <stdint.h>

struct chunk {
    char* body;
    long int length;
    long int parsed;
};


int
parse_chunks(uint8_t * buffer, size_t read, struct chunk*** chunks_read, int chunk_count);

#endif //PC_2018_07_TRANSFORMATION_chunk_H
