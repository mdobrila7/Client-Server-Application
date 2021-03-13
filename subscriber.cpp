#include <iostream>

#include <stdio.h>

#include <stdlib.h>

#include <cstring>

#include <vector>

#include <unistd.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <zconf.h>

#include <netinet/tcp.h>

#include <arpa/inet.h>

#include "defines.h"

using namespace std;

void usage(char * file) {
    fprintf(stderr, "Usage: %s ID_Client IP_Server PORT_Server\n", file);
    exit(0);
}

//functie de afisare a mesajului construit
void afisare(int type, char * topic, char * addr_port) {
    if (type == 0) {
        cout << addr_port << " - " << topic << " - INT - ";
    }
    if (type == 1) {
        cout << addr_port << " - " << topic << " - SHORT_REAL - ";
    }
    if (type == 2) {
        cout << addr_port << " - " << topic << " - FLOAT - ";
    }
    if (type == 3) {
        cout << addr_port << " - " << topic << " - STRING - ";
    }
}

// functie de parsare a mesajului in functie de tipul lui
void parsare(char * addr_port, char * buff) {
    char topic[51];
    strcpy(topic, buff);
    int type = (buff[50]);

    if (type == 0) {
        int value = int((unsigned char)(buff[52]) << 24 |
            (unsigned char)(buff[53]) << 16 |
            (unsigned char)(buff[54]) << 8 |
            (unsigned char)(buff[55]));

        if (buff[51])
            value = (-1) * value;

        afisare(type, topic, addr_port);
        cout << value << "\n";
    }

    if (type == 1) {
        double value = int((unsigned char)(buff[51]) << 8 |
            (unsigned char)(buff[52]));
        value /= 100;

        afisare(type, topic, addr_port);
        printf("%.2f\n", value);
    }

    if (type == 2) {
        double value = int((unsigned char)(buff[52]) << 24 |
            (unsigned char)(buff[53]) << 16 |
            (unsigned char)(buff[54]) << 8 |
            (unsigned char)(buff[55]));

        uint8_t mod = buff[56];

        while (mod) {
            value /= 10;
            mod -= 1;
        }

        if (buff[51])
            value = (-1) * value;

        afisare(type, topic, addr_port);
        printf("%f\n", value);
    }

    if (type == 3) {
        afisare(type, topic, addr_port);
        cout << buff + 51 << "\n";
    }
}

// cateva cazuri care pot fi intalnite cand se dau comenzi
int programare_defensiva(int socket, char * buff, char * topic) {
    int n, break_typo = 0;

    if (strncmp(buff, "unsubscribe", 11) == false) {
        n = send(socket, buff, strlen(buff), 0);
        if (n < 0) {
            fprintf(stderr, "Unable to send unsubscribe.\n");
            exit(0);
        }
        printf("unsubscribed %s\n", topic);
        break_typo = 1;

    } else if (!strncmp(buff, "subscribe", 9)) {
        n = send(socket, buff, strlen(buff), 0);
        if (n < 0) {
            fprintf(stderr, "Unable to send subscribe.\n");
            exit(0);
        }
        printf("subscribed %s\n", topic);
        break_typo = 1;
    } else if (!strncmp(buff, "exit", 4)) {
        close(socket);
        exit(0);
    } else {
        cout << "Wrong command.\n";
        break_typo = 1;
    }
    return break_typo;
}

int main(int argc, char * argv[]) {
    // variabile pentru client
    int sockfd, n, enable = 1, i = 0;
    struct sockaddr_in serv_addr;
    int fdmax, tmp;
    fd_set read_set, tmp_read_set;
    char buff[BUFLEN], ack[2], topic[51];
    char addr_port[20], command[12], SF[2];
    

    // programare defensiva
    DIE(argc != 4, "Usage: ./subscriber <ID> <IP_SERVER> <PORT_SERVER>.\n");

    DIE(strlen(argv[1]) > 10, "Subscriber ID should be at most 10 characters long.\n");

    // deschidem socket-ul clientului
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    // se dezactiveaza algoritmul lui Neagle pentru conexiunea la server
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char * ) & enable, sizeof(int));

    // initializam structura server-ului
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    DIE(inet_aton(argv[2], & serv_addr.sin_addr) == 0,
        "Incorrect <IP_SERVER>. Conversion failed.\n");

    // conexiunea cu server-ul
    DIE(connect(sockfd, (sockaddr * ) & serv_addr, sizeof(serv_addr)) < 0,
        "Connection refused.\n");

    // trimitere ID-ul
    DIE(send(sockfd, argv[1], strlen(argv[1]) + 1, 0) < 0, "Unable to send ID to server.\n");

    // initializam multimea de lucru
    FD_ZERO( & read_set);
    FD_SET(STDIN_FILENO, & read_set);
    FD_SET(sockfd, & read_set);
    fdmax = sockfd;

    memset(buff, 0, 1551);
    n = recv(sockfd, ack, sizeof(ack), 0);
    if (n < 0) {
        fprintf(stderr, "Unable to receive.\n");
        exit(0);
    }
    if (!strncmp(ack, "F", 1)) {
        fprintf(stderr, "This ID is already used.\n");
        exit(0);
    }

    while (1) {
        // multiplexarea propriu-zisa
        tmp_read_set = read_set;
        fdmax++;
        int ret = select(fdmax, & tmp_read_set, NULL, NULL, NULL);
        DIE(ret < 0, "Unable to select\n");

        tmp = 0;

        while (i <= sockfd) {
            if (FD_ISSET(i, & tmp_read_set)) {
                if (i == STDIN_FILENO) {
                    // clientul efectueaza o comanda
                    memset(buff, 0, BUFLEN);
                    fgets(buff, BUFLEN, stdin);

                    sscanf(buff, "%s %s %s", command, topic, SF);

                    // programare defensiva 
                    tmp = programare_defensiva(sockfd, buff, topic);
                    if (tmp == 1) {
                        break;
                    }
                }
            } else {
                // mesaj de la server
                memset(buff, 0, BUFLEN);
                n = recv(sockfd, buff, sizeof(buff), 0);
                if (n < 0) {
                    fprintf(stderr, "Unable to receive.\n");
                    exit(0);
                }

                // comanda de exit de la server
                if (n == 0) {
                    close(sockfd);
                    printf("Server closed.\n");
                    exit(0);
                } else {
                    // mesaj de la clientul UDP
                    memset(addr_port, 0, 20);
                    n = recv(sockfd, addr_port, sizeof(addr_port), 0);
                    if (n < 0) {
                        fprintf(stderr, "Unable to receive.\n");
                        exit(0);
                    }

                    // parsarea mesajului
                    parsare(addr_port, buff);
                    break;
                }
            }
            ++i;
        }
    }

    close(sockfd);
}
