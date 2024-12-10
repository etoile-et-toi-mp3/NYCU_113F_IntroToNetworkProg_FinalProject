#include "unp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_ID_LEN 12
#define MAX_CLIENT 200

enum State { NONE,
             FIRST,
             SECOND };

struct client_db {
    char id[MAX_ID_LEN];
    char TCP_servaddr[MAXLINE];
    enum State state;
    int nconnt;
} clients[MAX_CLIENT];

static void sig_alarm(int signo) {
    return;
}

int find_index(char *id) {
    int i;

    for (i = 0; i < MAX_CLIENT; i++)
    {
        if (strcmp(id, clients[i].id) == 0)
        {
            return i;
        }
    }
    return -1;
};

int find_first(char *id) {
    static int min_loc = 0;
    int i, loc = -1;

    for (i = 0; i < MAX_CLIENT; i++)
    {
        if (strcmp(id, clients[i].id) == 0)
        {
            loc = i;
            break;
        }
    }
    if (loc == -1)
    { // a new client
        if (min_loc < MAX_CLIENT)
        {
            loc = min_loc;
            strcpy(clients[loc].id, id);
            min_loc++;
        }
        else // no space to keep the new client
            loc = -1;
    }
    return loc;
};

int create_TCP_connections(int nconnt, char *TCP_servaddr, int TCP_servport, FILE *cli_fp, FILE *fp) {
    int i, x, y, n, clifd[11], local_port, port_no;
    char recvline[MAXLINE], buff[MAXLINE];
    struct sockaddr_in servaddr, tempaddr;
    socklen_t servlen, templen;

    /* prepare address structure for the TCP server */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TCP_servport);
    Inet_pton(AF_INET, TCP_servaddr, &servaddr.sin_addr);

    fprintf(fp, "Creating %d connections...\n", nconnt);

    for (i = 0; i < nconnt; i++)
    {
        /* create a file descriptor for client socket */
        clifd[i] = Socket(AF_INET, SOCK_STREAM, 0);
        servlen = sizeof(servaddr);
        /* create a TCP connection to the same TCP server */
        if (connect(clifd[i], (SA *)&servaddr, servlen) < 0)
        {
            if (cli_fp != NULL)
            {
                fprintf(cli_fp, "connection %d: connect error!\n", i);
                fflush(cli_fp);
            }
            else
            {
                fprintf(fp, "connection %d: connect error!\n", i);
                fflush(fp);
            };
            close(clifd[i]);
            return (-1);
        };
        templen = sizeof(tempaddr);
        x = rand() % 1024 + 1;
        snprintf(buff, sizeof(buff), "%d\n", x);
        Writen(clifd[i], buff, strlen(buff));

        /* get local port no */
        getsockname(clifd[i], (SA *)&tempaddr, &templen);
        local_port = ntohs(tempaddr.sin_port);
        if (cli_fp != NULL)
            fprintf(cli_fp, "Connection %d (port #%d): written %s",
                    i, local_port, buff);
        else
            fprintf(fp, "Connection %d (port #%d): written %s",
                    i, local_port, buff);
        if ((n = Readline(clifd[i], recvline, MAXLINE)) == 0)
        {
            if (cli_fp != NULL)
                fprintf(cli_fp, "no data received. quitting\n");
            else
                fprintf(fp, "no data received. quitting\n");
            close(clifd[i]);
            return (-1);
        };
        recvline[n] = 0;
        sscanf(recvline, "%d %d", &port_no, &y);

        if (port_no == local_port && x == y)
        {
            Writen(clifd[i], "ok\n", 3);
            close(clifd[i]);
            if (cli_fp != NULL)
                fprintf(cli_fp, "port and value match, ok sent!\n");
            else
                fprintf(fp, "port and value match, ok sent!\n");
        }
        else
        {
            Writen(clifd[i], "nak\n", 4);
            close(clifd[i]);
            if (cli_fp != NULL)
            {
                fprintf(cli_fp, "recv: %s", recvline);
                fprintf(cli_fp, "port or value not match, nak sent!\n");
            }
            else
            {
                fprintf(fp, "recv: %s", recvline);
                fprintf(fp, "port or value not match, nak sent!\n");
            }
            return (-1);
        };
    };
    return (0);
};

void get_udp_client(int sockfd, FILE *fp) {
    int n, nconnt, result, code, index;
    int TCP_servport;
    socklen_t clilen;
    struct sockaddr_in cliaddr;
    char recvline[MAXLINE], buff[MAXLINE], TCP_servaddr[MAXLINE];
    char student_id[MAX_ID_LEN], TCP_servIP_got[MAXLINE];
    FILE *cli_fp;
    time_t ticks;

    /* get a request from UDP client */
    clilen = sizeof(cliaddr);
    n = Recvfrom(sockfd, recvline, MAXLINE, 0, (SA *)&cliaddr, &clilen);
    recvline[n] = 0;
    ticks = time(NULL);
    Inet_ntop(AF_INET, &cliaddr.sin_addr, TCP_servaddr, MAXLINE);
    fprintf(fp, "\n[%.24s] recvfrom %s, port %d: %s\n",
            ctime(&ticks), TCP_servaddr, ntohs(cliaddr.sin_port), recvline);
    // parse the UDP segment
    if (sscanf(recvline, "%d %s %s", &code, student_id, TCP_servIP_got) != 3)
    {
        fprintf(fp, "Missing parameters (expected three)\n");
        return;
    }

    if (code == 11)
    { // first UDP segment
        fprintf(fp, "New Req. Student ID = %s, TCP Serv addr = %s\n",
                student_id, TCP_servIP_got);
        snprintf(buff, sizeof(buff), "%s.log", student_id);
        if ((cli_fp = fopen(buff, "wt")) == NULL)
        {
            fprintf(fp, "log file open error: %s\n", buff);
            //        	exit(0);
        }
        else
        { // cli_fp != NULL
            fprintf(cli_fp, "\n[%.24s] recvfrom %s, port %d: %s\n",
                    ctime(&ticks), TCP_servaddr, ntohs(cliaddr.sin_port), recvline);
        };

        if (strcmp(TCP_servaddr, TCP_servIP_got) != 0)
        { // student resides in NAT
            nconnt = 99;
        }
        else
        {
            /* find stored information */
            index = find_first(student_id);
            if (index != -1)
            {
                srand((int)ticks);
                nconnt = rand() % 10 + 1;
                strcpy(clients[index].TCP_servaddr, TCP_servaddr);
                clients[index].state = FIRST;
                clients[index].nconnt = nconnt;
            }
            else
                nconnt = 98;
        }
        /* reply to the UDP client the number of TCP connections to be created, the student ID, and the IP address of the TCP server */
        snprintf(buff, sizeof(buff), "%d %s %s", nconnt, student_id, TCP_servaddr);
        Sendto(sockfd, buff, strlen(buff), 0, (SA *)&cliaddr, clilen);
        if (cli_fp != NULL)
        {
            fprintf(cli_fp, "replied: %s\n", buff);
            fclose(cli_fp);
        }
        else
            fprintf(fp, "replied: %s\n", buff);
        return;
    };
    if (code == 13)
    {                                                // 2dn UDP segment
        sscanf(TCP_servIP_got, "%d", &TCP_servport); // get TCP server port

        index = find_index(student_id);
        if (index == -1 || clients[index].state != FIRST)
        {
            fprintf(fp, "student id = %s, index = %d\n", student_id, index);
            fprintf(fp, "got code 13 without previous code, quit!\n");
            return;
        };
        if (strcmp(TCP_servaddr, clients[index].TCP_servaddr) != 0)
        {
            fprintf(fp, "inconsistent TCP server address, quit!\n");
            return;
        };
        clients[index].state = SECOND;
        nconnt = clients[index].nconnt;
        snprintf(buff, sizeof(buff), "%s.log", student_id);
        if ((cli_fp = fopen(buff, "at")) == NULL)
        { // open log file for appending
            fprintf(fp, "log file open error: %s\n", buff);
            //        	exit(0);
        };
        if (cli_fp != NULL)
        {
            fprintf(cli_fp, "\n[%.24s] recvfrom %s, port %d: %s\n",
                    ctime(&ticks), TCP_servaddr, TCP_servport, recvline);
            fprintf(cli_fp, "got code 13, proceed\n");
        }
        else
            fprintf(fp, "got code 13, proceed\n");

        result = create_TCP_connections(nconnt, TCP_servaddr, TCP_servport, cli_fp, fp);

        ticks = time(NULL);
        if (cli_fp != NULL)
            fprintf(cli_fp, "[%.24s] sendto %s, port %d: ",
                    ctime(&ticks), TCP_servaddr, ntohs(cliaddr.sin_port));
        fprintf(fp, "[%.24s] sendto %s, port %d: ",
                ctime(&ticks), TCP_servaddr, ntohs(cliaddr.sin_port));
        if (result == -1)
        {
            Sendto(sockfd, "nak", 3, 0, (SA *)&cliaddr, clilen);
            if (cli_fp != NULL)
                fprintf(cli_fp, "nak.\n");
            fprintf(fp, "nak.\n");
        }
        else
        { // everything is fine
            Sendto(sockfd, "ok", 2, 0, (SA *)&cliaddr, clilen);
            if (cli_fp != NULL)
                fprintf(cli_fp, "ok.\n");
            fprintf(fp, "ok.\n");
        };
        if (cli_fp != NULL)
            fclose(cli_fp);
    };
    return;
}

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in servaddr;
    FILE *fp;

    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

    /* prepare address structure for this UDP server */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT + 3);

    Bind(sockfd, (SA *)&servaddr, sizeof(servaddr));

    if ((fp = fopen("ass4serv24.log", "a")) == NULL)
    {
        printf("log file open error!\n");
        exit(0);
    };

    Signal(SIGALRM, sig_alarm);

    do
    {
        get_udp_client(sockfd, fp);
        fflush(fp);
    } while (1);
}