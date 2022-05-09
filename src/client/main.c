#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "../deps/edutils.h"

#define DATABUFLEN 516
#define ACKBUFLEN 4
#define OPBUFLEN 100
#define FILEBUFLEN 200
#define PORT 69
#define MAXRETR 5
#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERR 5

#define DEBUG 1

struct tftp_gen {
        uint16_t op;
        char rest[DATABUFLEN - 2];
};

struct tftp_err {
        uint16_t op;
        uint16_t code;
        char message[DATABUFLEN - 4];
};

struct tftp_data {
        uint16_t op;
        uint16_t blocknum;
        char block[DATABUFLEN - 4];
};

struct tftp_wrq {
        uint16_t op;
        char *filename;
        char *mode;
};

struct tftp_ack {
        uint16_t op;
        uint16_t blocknum;
};

typedef int int32_t;
static const char *MODE = "octet";

static int put(int s, FILE *f, struct sockaddr_in saddr, char *filename);
static int send_wrq(int s, struct sockaddr_in saddr, char *filename);
static int get(int s, FILE *f, struct sockaddr_in saddr, char *filename);
static char clean_pred(char c);
static int send_rrq(int s, struct sockaddr_in saddr, char *filename);
static int send_ack(int s, struct sockaddr_in reply_addr, uint16_t blocknum);
static int send_data(int s, FILE *f, struct sockaddr_in reply_addr, uint16_t blocknum);
static int16_t write_data(FILE *f, struct tftp_gen gen, int genlen);
static int handle_err(struct tftp_gen gen);
static struct tftp_ack to_ack(struct tftp_gen *p);
static struct tftp_err to_err(struct tftp_gen *p);
static struct tftp_data to_data(struct tftp_gen *p);
static int gen_command(int s, struct sockaddr_in saddr, char *fmode, int func(int s, FILE *f, struct sockaddr_in saddr, char *fname));

int main(int argc, char **argv) {
        if (argc < 2) {
                printf("USAGE: client [ip]");
                return EXIT_FAILURE;
        }
        int s;
        struct sockaddr_in saddr;

        char op[OPBUFLEN];
        memset(op, 0, OPBUFLEN);

        CHECK((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)));
        saddr = get_address(argv[1], PORT);

        while (1) {
                printf("edvin_tftp > ");
                if (!fgets(op, OPBUFLEN, stdin))
                        return EXIT_FAILURE;

                if (!strncmp(op, "quit", 4)) {
                        break;
                }
                if (!strncmp(op, "put", 3)) {
                        gen_command(s, saddr, "r", &put);
                        continue;
                }

                if (!strncmp(op, "get", 3)) {
                        gen_command(s, saddr, "w", &get);
                        continue;
                }
        }

        return EXIT_SUCCESS;
}

static int gen_command(int s, struct sockaddr_in saddr, char *fmode, int func(int s, FILE *f, struct sockaddr_in saddr, char *fname)) {
        char fname[FILEBUFLEN];
        memset(fname, 0, FILEBUFLEN);
        FILE *f;
        char *clean_fname;

        if (!fgets(fname, FILEBUFLEN, stdin))
                return EXIT_FAILURE;

        if (!(clean_fname = clean_token(fname, FILEBUFLEN, &clean_pred)))
                return EXIT_FAILURE;

        if (!(f = fopen(clean_fname, fmode))) {
                fprintf(stderr, "ERROR (" __FILE__ ":%d) -- %s\n", __LINE__, strerror(errno));
                free(clean_fname);
                return EXIT_FAILURE;
        }

        CHECK_PRINT(func(s, f, saddr, clean_fname));

        fclose(f);
        free(clean_fname);
        return EXIT_SUCCESS;
}

static int put(int s, FILE *f, struct sockaddr_in saddr, char *filename) {
        struct sockaddr_in reply_addr;
        int addrlen = sizeof(reply_addr);
        char q = 0;
        int n;
        struct tftp_ack ack;
        struct tftp_gen gen;

        CHECK_RETURN(send_wrq(s, saddr, filename));

        for (uint16_t blocknum = 1; !q; blocknum++) {
                memset(&gen, 0, DATABUFLEN);
                CHECK_RETURN(recvfrom(s, &gen, DATABUFLEN, 0, (struct sockaddr *)&reply_addr, &addrlen));
                if (handle_err(gen))
                        return EXIT_FAILURE;

                ack = to_ack(&gen);
                ack.blocknum = htons(ack.blocknum);
                DEBUG_PRINT(("ACK received for block number %d.\n", ack.blocknum));
                if (ack.blocknum != blocknum - 1)
                        return EXIT_FAILURE;

                CHECK_RETURN(n = send_data(s, f, reply_addr, blocknum));
                q = n < DATABUFLEN - 4;
        }

        CHECK_RETURN(recvfrom(s, &gen, DATABUFLEN, 0, (struct sockaddr *)&reply_addr, &addrlen));
        if (handle_err(gen))
                return EXIT_FAILURE;

        printf("Transfer complete.\n");
        return EXIT_SUCCESS;
}

static int send_data(int s, FILE *f, struct sockaddr_in reply_addr, uint16_t blocknum) {
        struct tftp_data data;
        int n;

        memset(&data, 0, sizeof(struct tftp_data));
        data.op = htons(DATA);
        data.blocknum = htons(blocknum);
        n = fread(data.block, sizeof(char), DATABUFLEN - 4, f);
        DEBUG_PRINT(("Sending block %d of %d bytes.\n", blocknum, n));
        CHECK_RETURN(n = sendto(s, &data, n + 4, 0, (struct sockaddr *)&reply_addr, sizeof(reply_addr)));

        return n;
}

static int get(int s, FILE *f, struct sockaddr_in saddr, char *filename) {
        struct tftp_gen gen;
        struct sockaddr_in reply_addr;
        int addrlen = sizeof(reply_addr);
        int genlen;
        uint16_t blocknum;

        CHECK_RETURN(send_rrq(s, saddr, filename));
        do {
                memset(&gen, 0, DATABUFLEN);
                CHECK(genlen = recvfrom(s, &gen, DATABUFLEN, 0, (struct sockaddr *)&reply_addr, &addrlen));
                if (handle_err(gen))
                        return EXIT_FAILURE;
                CHECK_RETURN(blocknum = write_data(f, gen, genlen));
                CHECK_RETURN(send_ack(s, reply_addr, blocknum));
        } while (genlen == DATABUFLEN);

        printf("Transfer complete.\n");
        return EXIT_SUCCESS;
}

static int send_wrq(int s, struct sockaddr_in saddr, char *filename) {
        int req_len;
        char message[DATABUFLEN];
        int filename_len;

        filename_len = strlen(filename);
        memset(message, 0, DATABUFLEN);
        message[1] = WRQ;
        strcpy(message + 2, filename);
        strcpy(message + 2 + filename_len + 1, MODE);
        req_len = 2 + strlen(filename) + 1 + strlen(MODE) + 1;
        CHECK_RETURN(sendto(s, message, req_len, 0, (struct sockaddr *)&saddr, sizeof(saddr)));
        DEBUG_PRINT(("Sent WRQ.\n"));

        return 1;
}

static int send_rrq(int s, struct sockaddr_in saddr, char *filename) {
        int req_len;
        char message[DATABUFLEN];
        int filename_len;

        filename_len = strlen(filename);
        memset(message, 0, DATABUFLEN);
        message[1] = RRQ;
        strcpy(message + 2, filename);
        strcpy(message + 2 + filename_len + 1, MODE);
        req_len = 2 + strlen(filename) + 1 + strlen(MODE) + 1;
        CHECK_RETURN(sendto(s, message, req_len, 0, (struct sockaddr *)&saddr, sizeof(saddr)));
        DEBUG_PRINT(("Sent RRQ.\n"));

        return 1;
}

static int send_ack(int s, struct sockaddr_in reply_addr, uint16_t blocknum) {
        struct tftp_ack ack;
        memset(&ack, 0, sizeof(struct tftp_ack));
        ack.op = htons(ACK);
        ack.blocknum = blocknum;
        DEBUG_PRINT(("Sending ACK for block %d.\n", htons(ack.blocknum)));
        CHECK_RETURN(sendto(s, &ack, sizeof(struct tftp_ack), 0, (struct sockaddr *)&reply_addr, sizeof(reply_addr)));

        return 1;
}

static int16_t write_data(FILE *f, struct tftp_gen gen, int genlen) {
        if (htons(gen.op) != DATA)
                return -1;

        struct tftp_data data;
        int blocklen;
        int n;

        memset(&data, 0, DATABUFLEN);
        blocklen = genlen - 4;
        data = to_data(&gen);
        DEBUG_PRINT(("Block %d of %d bytes received.\n", htons(data.blocknum), blocklen));

        fwrite(data.block, sizeof(char), blocklen, f);
        if (ferror(f)) {
                printf("Error writing file");
                return -1;
        }

        return data.blocknum;
}

static int handle_err(struct tftp_gen gen) {
        if (htons(gen.op) != ERR)
                return 0;

        struct tftp_err err;

        err = to_err(&gen);
        printf("Error code: %d -- %s\n", htons(err.code), err.message);
        return 1;
}

static char clean_pred(char c) {
        return isalnum(c) || c == '.' || c == '/' || c == '-' || c == '_';
}

static struct tftp_ack to_ack(struct tftp_gen *p) {
        return *(struct tftp_ack *)p;
}

static struct tftp_err to_err(struct tftp_gen *p) {
        return *(struct tftp_err *)p;
}

static struct tftp_data to_data(struct tftp_gen *p) {
        return *(struct tftp_data *)p;
}
