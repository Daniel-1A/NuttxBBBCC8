#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/time.h>

#define PORT        5000
#define BUF_SZ      512
#define C_SPORT     40000

// Función para calcular el checksum de 16 bits
uint16_t cksum16(const void *v, size_t n) {
    const uint8_t *p = (const uint8_t*)v;
    uint32_t acc=0;
    while (n > 1) {
        acc += (p[0] << 8) | p[1];
        p += 2;
        n -= 2;
        if (acc > 0xffff) acc = (acc & 0xffff) + 1;
    }
    if (n) {
        acc += p[0] << 8;
        if (acc > 0xffff) acc = (acc & 0xffff) + 1;
    }
    return ~acc;
}

// Función para calcular el checksum de IP
uint16_t ip_checksum(const void *h) {
    return cksum16(h, 20);
}

// Función del servidor (solo escucha y responde)
void *server_thread(void *arg) {
    int s = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (s < 0) {
        perror("socket creation failed");
        return NULL;
    }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(PORT);
    srv.sin_addr.s_addr = htonl(INADDR_ANY);  // Esto no es necesario para raw sockets

    // NO necesitamos bind() para raw sockets
    // bind(s, (struct sockaddr *)&srv, sizeof(srv)); // Comentado porque no es necesario

    printf("Servidor RAW en 127.0.0.1:%d\n", PORT);

    for (;;) {
        char buf[BUF_SZ];
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        ssize_t n = recvfrom(s, buf, sizeof(buf)-1, 0, (struct sockaddr *)&cli, &clen);
        if (n <= 0) continue;
        buf[n] = 0;
        printf("RX servidor: %s", buf);
        sendto(s, buf, n, 0, (struct sockaddr *)&cli, clen); // Echo
    }
    close(s);
    return NULL;
}

// Función del cliente (envía y recibe datos)
void *client_thread(void *arg) {
    struct in_addr src, dst;
    inet_pton(AF_INET, "127.0.0.1", &src);
    inet_pton(AF_INET, "127.0.0.1", &dst);

    int raw = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (raw < 0) {
        perror("socket creation failed");
        return NULL;
    }

    struct sockaddr_in to = {0};
    to.sin_family = AF_INET;
    to.sin_port = htons(PORT);
    to.sin_addr = dst;

    printf("Cliente RAW origen 127.0.0.1:%d -> 127.0.0.1:%d\n", C_SPORT, PORT);
    printf("Escribe y Enter. CTRL+D termina.\n");

    char line[BUF_SZ];
    while (fgets(line, sizeof(line), stdin)) {
        size_t paylen = strlen(line);

        // Construir el encabezado IP
        struct {
            uint8_t ver_ihl, tos;
            uint16_t tot_len, id, frag_off;
            uint8_t ttl, proto;
            uint16_t check;
            uint32_t saddr, daddr;
        } __attribute__((packed)) iph;
        memset(&iph, 0, sizeof(iph));
        iph.ver_ihl = (4 << 4) | 5;  // IP version 4, header length 5
        iph.tot_len = htons(20 + 8 + paylen);  // Total length (IP + UDP + data)
        iph.id = htons(1);
        iph.frag_off = htons(0x4000);  // No fragmentation
        iph.ttl = 64;
        iph.proto = IPPROTO_UDP;
        iph.saddr = src.s_addr;
        iph.daddr = dst.s_addr;
        iph.check = ip_checksum(&iph);

        // Construir el encabezado UDP
        struct {
            uint16_t sport, dport, len, csum;
        } __attribute__((packed)) udph;
        udph.sport = htons(C_SPORT);
        udph.dport = htons(PORT);
        udph.len = htons(8 + paylen);
        udph.csum = 0;  // Sin checksum por simplicidad

        uint8_t pkt[20 + 8 + BUF_SZ];
        memcpy(pkt, &iph, 20);
        memcpy(pkt + 20, &udph, 8);
        memcpy(pkt + 28, line, paylen);

        // Enviar el paquete RAW
        sendto(raw, pkt, 20 + 8 + paylen, 0, (struct sockaddr *)&to, sizeof(to));
        printf("TX %s:%d -> %s:%d bytes=%zu\n", inet_ntoa(src), C_SPORT, inet_ntoa(dst), PORT, paylen);

        // Intentar leer respuesta
        struct sockaddr_in from;
        socklen_t flen = sizeof(from);
        char rbuf[BUF_SZ];
        ssize_t m = recvfrom(raw, rbuf, sizeof(rbuf) - 1, 0, (struct sockaddr *)&from, &flen);
        if (m > 0) {
            rbuf[m] = 0;
            printf("Eco cliente: %s", rbuf);
        }
    }

    close(raw);
    return NULL;
}

// Función principal
int main(void) {
    pthread_t tsv, tcl;
    pthread_create(&tsv, NULL, server_thread, NULL);
    pthread_create(&tcl, NULL, client_thread, NULL);
    pthread_join(tsv, NULL);
    pthread_join(tcl, NULL);
    return 0;
}
