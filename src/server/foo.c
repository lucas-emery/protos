#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>

#include "buffer.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

static void serve(int *fds);
static int init(int *fds, char * bin, int infd, int outfd);
void *transform(void**);

void * transform(void ** args){
    char * bin =  (char *) args[0];
    int infd = *((int *) args[1]);
    int outfd = *((int *) args[2]);

    printf("reading from %d\n", infd);
    printf("writing to %d\n", outfd);

    int fds[] = { -1, -1, -1, -1};
    if(init(fds,bin,infd,outfd) != 0){
        perror("forking");
        return NULL;
    }
    serve(fds);
    printf("finished\n");
    return NULL;
}

static int init(int * fds, char * bin, int infd, int outfd){
    enum { // enum utilizado para identificar las puntas de los pipes.
        R  = 0,
        W  = 1,
    };

    int ret = 0;

    int in [] = { -1, -1};    // pipe de entrada
    int out[] = { -1, -1};    // pipe de salida


    if(pipe(in) == -1 || pipe(out) == -1) {
        perror("creating pipes");
        exit(EXIT_FAILURE);
        return EXIT_FAILURE;
    }

    const pid_t cmdpid = fork();
    if (cmdpid == -1) {
       perror("creating process for user command");
       exit(EXIT_FAILURE);
       return EXIT_FAILURE;
    } else if (cmdpid == 0) {
        // en el hijo debemos reemplazar stdin y stdout por los pipes antes
        // de ejecutar el comando.
        close(infd);
        close(outfd);
        close(in [W]);
        close(out[R]);
        in [W] = out[R] = -1;
        dup2(in [R], infd);
        dup2(out[W], outfd);
        if(-1 == execv(bin, (char **) 0)) {
            perror("executing command");
            close(in [R]);
            close(out[W]);
            ret = 1;
        }
        exit(ret);
    } else {
        close(in [R]);
        close(out[W]);
        in[R] = out[W] = -1;

        fds[0] = infd;
        fds[1] = outfd;
        fds[2] = in[W];
        fds[3] = out[R];
    }

    return ret;
}

/**
 * Calcula la paridad de byte de un arreglo `ptr' que tiene `n' elemenos.
 * Actualiza el valor en *parity.
 */
static void
parity(const uint8_t *ptr, const ssize_t n, uint8_t *parity) {
    for(ssize_t i = 0; i < n; i++) {
        *parity ^= ptr[i];
    }
}

/**
 * Lee bytes desde *fd, dejando el resultado en *buff, y calculando
 * opcionalmente la paridad.
 */
static int
doread(int *fd, struct buffer *buff, uint8_t *par) {
    uint8_t *ptr;
    ssize_t  n;
     size_t  count = 0;
        int    ret = 0;

    ptr = buffer_write_ptr(buff, &count);
    n = read(*fd, ptr, count);
    if(n == 0 || n == -1) {
        printf("not reading anymore from %d\n", *fd);
        *fd = -1;
        ret = -1;
    } else {
        if(NULL != par) {
            parity(ptr, n, par);
        }
        buffer_write_adv(buff, n);
    }

    return ret;
}

/**
 * Escribe bytes de buff, en *fd, calculando la paridad a byte opcionalmente.
 */
static int
dowrite(int *fd, struct buffer *buff, uint8_t *par) {
    uint8_t *ptr;
    ssize_t  n;
     size_t  count = 0;
        int    ret = 0;

    ptr = buffer_read_ptr(buff, &count);
    n = write(*fd, ptr, count);
    if (n == -1) {
        *fd = -1;
        ret = -1;
    } else {
        if(NULL != par) {
            parity(ptr, n, par);
        }
        buffer_read_adv(buff, n);
    }

    return ret;
}

enum {
    EX_R,   // external read
    EX_W,   // external write
    CH_W,   // children write
    CH_R,   // children read
};

/**
 * Realiza toda la corogreafia de copia de bytes entre 4 file descriptors.
 *
 * Recibe un arreglo con los siguientes filedescriptors
 * fds[EX_R] = entrada del servidor
 * fds[EX_W] = salida al cliente
 * fds[CH_W] = escritura hacia el hijo
 * fds[CH_R] = lectra desde el hijo
 *
 * Y realiza las siguientes copias:
 *
 *   fds[EX_R]        fds[CH_W]
 *  ---read----> bin ---write-->
 *
 *   fds[EX_W]        fds[CH_R]
 *  <--write---- bou <---read---
 *
 */
static void
serve(int *fds) {
    // buffer para flujo de entrada, y para flujo salida
    uint8_t buff_in[4096] = {0}, buff_out[4096] = {0};
    struct buffer bin, bou;
    buffer_init(&bin, N(buff_in),  buff_in);
    buffer_init(&bou, N(buff_out), buff_out);


    // paridades
    uint8_t parity_in = 0x00, parity_out = 0x00;

    do {
        // Cálculo del primer argumento de select.
        int nfds = 0;
        for(unsigned i = 0 ; i < 4; i++) {
            if(fds[i] > nfds) {
                nfds = fds[i];
            }
        }
        nfds += 1;

        // Cálcúlo de intereses de lectura y escritura basándose en
        // los estados de los buffers y la existencia de file descriptors
        fd_set readfds, writefds;
        FD_ZERO(&readfds); FD_ZERO(&writefds);
        if(fds[EX_R] != -1 && buffer_can_write(&bin)) {
            FD_SET(fds[EX_R], &readfds);
        }
        if(fds[CH_R] != -1 && buffer_can_write(&bou)) {
            FD_SET(fds[CH_R], &readfds);
        }
        if(fds[CH_W] != -1 && buffer_can_read(&bin)) {
            FD_SET(fds[CH_W], &writefds);
        }
        if(fds[EX_W] != -1 && buffer_can_read(&bou)) {
            FD_SET(fds[EX_W], &writefds);
        }

        int n = select(nfds, &readfds, &writefds, NULL, NULL);
        if(n == -1) {
            perror("while selecting");
            break;
        } else if(n == 0) {
            // timeout... nada por hacer
        } else {
            if(FD_ISSET(fds[EX_R], &readfds)) { //read from server
                printf("selected servR\n");
                doread(fds + EX_R, &bin, &parity_in);

            }
            if(FD_ISSET(fds[CH_R], &readfds)) { //read from child
                printf("selected chR\n");
                doread(fds + CH_R, &bou, NULL);
            }
            if(FD_ISSET(fds[CH_W], &writefds)) { //write to child
                printf("selected chW\n");
                dowrite(fds + CH_W, &bin, NULL);
            }
            if(FD_ISSET(fds[EX_W], &writefds)) { //write to server
                printf("selected clientW\n");
                dowrite(fds + EX_W, &bou, &parity_out);
            }

            // si ya no podemos leer, dejamos de escribir
            if(-1 == fds[EX_R] && -1!= fds[CH_W] && !buffer_can_read(&bin)) {
                printf("closing\n");
                close(fds[CH_W]);
                fds[CH_W] = -1;
            }
            if(-1 == fds[CH_R] && -1!= fds[EX_W] && !buffer_can_read(&bou)) {
                printf("closing\n");
                close(fds[EX_W]);
                fds[EX_W] = -1;
            }
        }

        // śi ya no podemos leer ni escribir
        // retornamos
        if(-1 == fds[EX_R] && -1 == fds[CH_R]
        && -1 == fds[EX_W] && -1 == fds[CH_W]) {
            break;
        }
    } while(1);

    fprintf(stderr, "in  parity: 0x%02X\n", parity_in);
    fprintf(stderr, "out parity: 0x%02X\n", parity_out);
}
