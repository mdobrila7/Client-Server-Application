#include <iostream>

#include <vector>

#include <queue>

#include <stdio.h>

#include <stdlib.h>

#include <cstring>

#include <unistd.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include<bits/stdc++.h>

#include "defines.h"


using namespace std;

void usage(char * file) {
    fprintf(stderr, "Usage: %s PORT_Server\n", file);
    exit(0);
}

struct Topic {

    char name[51];
    int SF;
    int status;
};

struct Client {
    int sockfd;
    int ID;
    int status;
    vector < Topic > subscribed_topics;
    queue < char * > missed_posts;
};

struct Client add_new_client(int ID, int newsockfd) {

    Client new_client;
    new_client.ID = ID;
    new_client.sockfd = newsockfd;
    new_client.status = 1;

    return new_client;
}

struct Topic add_new_topic(char * topic, char * SF) {

    Topic new_topic;
    strcpy(new_topic.name, topic);
    new_topic.SF = atoi(SF);
    new_topic.status = 1;

    return new_topic;
}

int main(int argc, char * argv[]) {

    // Variabile utilizate de server
    int tcp_sockfd, udp_sockfd, newsockfd;
    int port_num, ID, n, i, ret, enable = 1;
    char buff[BUFLEN], topic[51], data_type[2];
    fd_set read_fds; // multimea de citire
    fd_set tmp_fds; // multime folosita temporar
    int fdmax; // valoare maxima fd din multimea read_fds
    char message[1501], command[12], SF[2];
    struct sockaddr_in serv_addr, tcp_cli_addr, udp_cli_addr;
    
    vector < Client > clients;
    

    // utilizam programarea defensiva
    DIE(argc != 2, "Usage: ./subscriber <ID> <IP_SERVER> <PORT_SERVER>.\n");

    // stocam port-ul server-ului
    port_num = atoi(argv[1]);
    DIE(port_num == 0, "atoi");

    // Cream si initializam socket-ul pentru lucrul cu clientul UDP
    udp_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "Unable to create UDP socket.\n");

    // Cream si initializam socket-ul pentru lucrul cu clientul TCP
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "Unable to create TCP socket.\n");

    // golim, apoi initializam structura server-ului
    memset((char * ) & serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_num);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // legarea la server
    ret = bind(tcp_sockfd, (struct sockaddr * ) & serv_addr, sizeof(struct sockaddr));
    if (ret < 0) {
        fprintf(stderr, "Bind error.\n");
        exit(0);
    }

    ret = listen(tcp_sockfd, MAX_CLIENTS);
    if (ret < 0) {
        fprintf(stderr, "Listen to server error.\n");
        exit(0);
    }

    n = setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, & enable, sizeof(int));
    if (n < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    // legam socket-ul la adresa server-ului
    n = bind(udp_sockfd, (const struct sockaddr * ) & serv_addr, sizeof(serv_addr));
    if (n < 0) {
        perror("bind failed.\n");
    }

    // se golesc multimile read_fds si tmp_fds
    FD_ZERO( & read_fds);
    FD_ZERO( & tmp_fds);

    // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
    FD_SET(tcp_sockfd, & read_fds);
    FD_SET(udp_sockfd, & read_fds);
    FD_SET(STDIN_FILENO, & read_fds);
    fdmax = tcp_sockfd + 1;

    while (1) {
        tmp_fds = read_fds;

        // multiplexarea
        fdmax++;
        ret = select(fdmax, & tmp_fds, NULL, NULL, NULL);
        if (ret < 0) {
            fprintf(stderr, "select.\n");
            exit(0);
        }
        socklen_t client_length;
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, & tmp_fds)) {
                if (i == STDIN_FILENO) {
                    // Comanda exit
                    memset(buff, 0, BUFLEN);
                    fgets(buff, BUFLEN, stdin);

                    if (!strncmp(buff, "exit", 4)) {
                        for (int j = 0; j <= fdmax; j++) {
                            if (FD_ISSET(j, & tmp_fds)) {
                                for (unsigned int k = 0; k < clients.size(); k++) {
                                    if (clients[k].sockfd == j) {
                                        n = send(j, "F", 1, 0);
                                        if (n < 0) {
                                            fprintf(stderr, "Unable to send False flag.\n");
                                            exit(0);
                                        }
                                    }
                                }
                                close(j);
                            }
                        }
                        FD_ZERO( & read_fds);
                        FD_ZERO( & tmp_fds);
                        exit(0);
                    }
                } else if (i == tcp_sockfd) {
                    //Serverul accepta o cerere de conexiune pe socket-ul cu listen
                    client_length = sizeof(tcp_cli_addr);
                    newsockfd = accept(tcp_sockfd, (struct sockaddr * ) & tcp_cli_addr, & client_length);
                    if (n < 0) {
                        fprintf(stderr, "Unable to accept.\n");
                        exit(0);
                    }

                    // Adaugare nou socket intors de accept la multimea de citire
                    FD_SET(newsockfd, & read_fds);
                    if (newsockfd > fdmax) {
                        fdmax = newsockfd;
                    }

                    int okay = 1;
                    int recon = 0;

                    // Se primeste ID-ul clientului
                    memset(buff, 0, 1551);
                    n = recv(newsockfd, buff, sizeof(buff), 0);
                    if (n < 0) {
                        fprintf(stderr, "Unable to receive.\n");
                        exit(0);
                    }
                    buff[n] = '\0';
                    ID = atoi(buff);

                    for (unsigned int j = 0; j < clients.size(); j++) {
                        if (clients[j].ID == ID) {
                            if (clients[j].status == 1) {
                                // ID-ul este deja folosit de un user online
                                okay = 0;
                                break;
                            } else {
                                // ID-ul a fost folosit si acum se reconecteaza
                                clients[j].sockfd = newsockfd;
                                recon = 1;

                                clients[j].status = 1;
                                break;
                            }
                        }
                    }

                    // Daca ID-ul e deja folosit, trimitem mesaj clientului TCP pentru a se reloga cu alt ID
                    if (okay) {
                        if (!recon) {
                            // T - flag TRUE pentru a arata ca se poate conecta
                            n = send(newsockfd, "T", 1, 0);
                            if (n < 0) {
                                fprintf(stderr, "Unable to send True flag.\n");
                                exit(0);
                            }
                        } else {
                            // R - flag RECONNECT pentru a arata ca se poate reconecta
                            n = send(newsockfd, "R", 1, 0);
                            if (n < 0) {
                                fprintf(stderr, "Unable to send Reconnect flag.\n");
                                exit(0);
                            }
                        }

                    } else {
                        // F - flag FALSE pentru a arata ca este deja un user online
                        send(newsockfd, "F", 1, 0);
                        FD_CLR(newsockfd, & read_fds);
                        close(newsockfd);
                        break;
                    }

                    cout << "New client " << ID << " connected from address " << inet_ntoa(tcp_cli_addr.sin_addr) << ":" << ntohs(tcp_cli_addr.sin_port) << '\n';

                    // Adaugam noul client in vectorul de clienti
                    clients.push_back(add_new_client(ID, newsockfd));

                } else if (i == udp_sockfd) {
                    // Mesaje de la clientul UDP
                    memset(buff, 0, 1551);
                    client_length = sizeof(udp_cli_addr);
                    memset( & udp_cli_addr, 0, sizeof(udp_cli_addr));
                    n = recvfrom(udp_sockfd, (char * ) buff, sizeof(buff), 0, (struct sockaddr * ) & udp_cli_addr, & client_length);

                    // Trimiterea mesajelor catre clientii TCP abonati la topic-ul respectiv
                    sscanf(buff, "%s %s %s", topic, data_type, message);

                    for (unsigned int j = 0; j < clients.size(); j++) {
                        for (unsigned int k = 0; k < clients[j].subscribed_topics.size(); k++) {

                            if (!strcmp(clients[j].subscribed_topics[k].name, topic) && clients[j].subscribed_topics[k].status == 1) {
                                char addr_port[20] = "";
                                strcat(addr_port, inet_ntoa(udp_cli_addr.sin_addr));
                                strcat(addr_port, ":");
                                strcat(addr_port, to_string(ntohs(udp_cli_addr.sin_port)).c_str());

                                if (clients[j].status == 0 && clients[j].subscribed_topics[k].SF == 1) {
                                    char * new_buff = (char * ) malloc(BUFLEN * sizeof(char));
                                    memcpy(new_buff, buff, BUFLEN);
                                    char * new_addr = (char * ) malloc(20 * sizeof(char));
                                    memcpy(new_addr, addr_port, 20);

                                    clients[j].missed_posts.push(new_buff);
                                    clients[j].missed_posts.push(new_addr);

                                } else if (clients[j].status == 1) {
                                    // Trimiterea mesajelor
                                    n = send(clients[j].sockfd, buff, sizeof(buff), 0);
                                    if (n < 0) {
                                        fprintf(stderr, "Unable to send message.\n");
                                        exit(0);
                                    }

                                    n = send(clients[j].sockfd, addr_port, sizeof(addr_port), 0);
                                    if (n < 0) {
                                        fprintf(stderr, "Unable to find port.\n");
                                        exit(0);
                                    }
                                }
                            }
                        }
                    }

                } else {
                    // TCP command
                    memset(buff, 0, 1551);
                    n = recv(i, buff, sizeof(buff), 0);
                    if (n < 0) {
                        fprintf(stderr, "Server closed.\n");
                        exit(0);
                    }

                    if (n == 0) {
                        // Conexiunea este close.
                        for (unsigned int j = 0; j < clients.size(); j++) {
                            if (clients[j].sockfd == i) {
                                clients[j].status = 0;

                                ID = clients[j].ID;
                                break;
                            }
                        }
                        printf("Client %d disconnected from server\n", ID);
                        close(i);
                        FD_CLR(i, & read_fds);
                    } else {
                        // Se primesc comenzile de la tastatura
                        sscanf(buff, "%s %s %s", command, topic, SF);

                        for (unsigned int j = 0; j < clients.size(); j++) {
                            if (clients[j].sockfd == i) {
                                if (!strncmp(command, "unsubscribe", 11)) {
                                    // Unsubscribe
                                    for (unsigned int k = 0; k < clients[j].subscribed_topics.size(); k++) {
                                        if (!strcmp(clients[j].subscribed_topics[k].name, topic)) {
                                            clients[j].subscribed_topics[k].status = 0;
                                            break;
                                        }
                                    }
                                } else {

                                    // sSbscribe
                                    int resubbed = 0;
                                    for (unsigned int k = 0; k < clients[j].subscribed_topics.size(); k++) {
                                        // Daca am fost anterior abonat, doar ma reabonez
                                        if (!strcmp(clients[j].subscribed_topics[k].name, topic)) {
                                            clients[j].subscribed_topics[k].status = 1;
                                            clients[j].subscribed_topics[k].SF = atoi(SF);
                                            resubbed = 1;
                                            break;
                                        }
                                    }
                                    if (!resubbed) {

                                        clients[j].subscribed_topics.push_back(add_new_topic(topic, SF));

                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    close(tcp_sockfd);
    close(udp_sockfd);
}
