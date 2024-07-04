//#include <QCoreApplication>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define RECV_DATA_SIZE 256
#define SEND_DATA_SIZE 181
#define MAX_CLIENTS 1

#define TCP_PORT_IN 4000

#define UDP_PORT_IN  9990
#define UDP_PORT_OUT 9991

#define POLL_PERIOD 5

int tcp_recv_sock = 0;

int udp_recv_sock = 0;
int udp_send_sock = 0;

fd_set readfds;
int max_fd = 0;

long long recv_time = 0;

typedef struct _Client
{
    int sock;
    in_addr addr;
} Client;

Client clients[MAX_CLIENTS];

unsigned char recv_data[RECV_DATA_SIZE] = {};
unsigned char send_data[SEND_DATA_SIZE] = \
  {0x43, 0x2c, 0xbb, 0xe7, 0xc0, 0x2c, 0x3b, 0x8e, 0x22, 0x4b, 0x9d, 0x9e, 0xcf, 0x1a, 0xc5, 0x39, \
   0x7c, 0x9f, 0xde, 0xf8, 0x32, 0xe,  0x9b, 0x7a, 0x9,  0x5d, 0xf5, 0xb3, 0x36, 0xc8, 0xa4, 0x10, \
   0x3a, 0xd3, 0xe2, 0x89, 0x63, 0xec, 0xec, 0xd9, 0xbb, 0x5,  0x4d, 0x46, 0xb5, 0x97, 0xfe, 0x47, \
   0xa7, 0xc9, 0x8b, 0x2c, 0x80, 0x97, 0x94, 0x90, 0xbc, 0xeb, 0x1b, 0x7b, 0xdd, 0xb9, 0xef, 0xd2, \
   0xda, 0x87, 0xe3, 0x56, 0x1f, 0xf5, 0x3,  0x17, 0x82, 0xc9, 0x46, 0x99, 0xb,  0x6d, 0xbb, 0xb7, \
   0x85, 0x0,  0x15, 0xc8, 0xfb, 0xf6, 0x3a, 0x38, 0x58, 0xe4, 0x9,  0x4c, 0x7f, 0xea, 0x15, 0x78, \
   0x43, 0x34, 0x44, 0x67, 0x5e, 0xee, 0x4e, 0xab, 0xe4, 0x3e, 0xd4, 0xb4, 0xcc, 0x9b, 0xdb, 0xdf, \
   0xd8, 0x65, 0x81, 0xcd, 0x18, 0x18, 0x55, 0xa4, 0xb9, 0x5e, 0x22, 0x5,  0x54, 0x7,  0xba, 0xb,  \
   0x27, 0x1f, 0xdb, 0xf1, 0x5f, 0xab, 0x9b, 0x7f, 0xd6, 0xe8, 0x92, 0xc1, 0xca, 0x27, 0xa4, 0x7f, \
   0x46, 0x3d, 0x25, 0xdb, 0x4a, 0xd7, 0x57, 0x64, 0xa6, 0x65, 0x1a, 0xcb, 0xa1, 0xe4, 0x80, 0x70, \
   0x6c, 0xb0, 0x43, 0x98, 0xea, 0xc3, 0x1f, 0xf8, 0x9a, 0xf6, 0x70, 0xa2, 0xc8, 0xb1, 0x67, 0xf1, \
   0x77, 0x69, 0x5f, 0x7d, 0x20};

long long get_time_ms(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

void print_buff(unsigned char *buf, int buf_len) {
    int i = 1;
    while(buf_len > 0) {
        printf("0x%02x, ", *buf);
        if (i % 16 == 0) {
            printf("\n");
        }
        buf_len--;
        buf++;
        i++;
    }
    printf("\n");
}

int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

int create_tcp_lsn_socket(int port)
{
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bindResult = bind(server_socket, (struct sockaddr *)&addr, sizeof(addr));
    if (bindResult == -1)
    {
        perror("TCP listener bindResult");
    }

    int listenResult = listen(server_socket, 5);
    if (listenResult == -1)
    {
        perror("TCP listener listenResult");
    }
    printf("TCP listener started\n");
    return server_socket;
}

int wait_client(int tcp_recv_sock) {
    max_fd = tcp_recv_sock;

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(tcp_recv_sock, &readfds);

        int selectResult = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        struct sockaddr_in cliaddr;
        socklen_t addrlen = sizeof(cliaddr);

        if (selectResult > 0) {
            if (FD_ISSET(tcp_recv_sock, &readfds))
            {
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i].sock == 0)
                    {
                        accept(tcp_recv_sock, (struct sockaddr *)&cliaddr, &addrlen);
                        clients[i].addr = cliaddr.sin_addr;
                        printf("Accept success, IP => %s\n", inet_ntoa(cliaddr.sin_addr));
                        return i;
                    }
                }
            }
        }
    }
}

int create_udp_recv_socket(int client_id)
{
    int udp_recv_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT_IN);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //inet_aton(inet_ntoa(clients[client_id].addr), &(addr.sin_addr));

    int bindResult = bind(udp_recv_sock, (struct sockaddr *)&addr, sizeof(addr));
    if (bindResult == -1)
    {
        perror("UDP receiver bindResult");
        return -1;
    }

    printf("UDP receiver started\n");

    return udp_recv_sock;
}

void get_message()
{
    struct sockaddr_in addr;

    FD_ZERO(&readfds);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].sock > 0)
        {
            FD_SET(clients[i].sock, &readfds);
            max_fd = max_fd + clients[i].sock;
        }
    }

    int select_result = select(max_fd + 1, &readfds, NULL, NULL, NULL);

    if (select_result > 0) {
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (FD_ISSET(clients[i].sock, &readfds))
            {
                addr.sin_family = AF_INET;
                addr.sin_port = htons(UDP_PORT_IN);
                inet_aton(inet_ntoa(clients[i].addr), &(addr.sin_addr));
                socklen_t addr_size = sizeof(addr);

                int size = recvfrom(clients[i].sock, recv_data, RECV_DATA_SIZE, 0, (struct sockaddr *)&addr, &addr_size);
                if (size == -1 || size == 0)
                {
                    printf("Closing UDP receiver\n");
                    clients[i].sock = 0;
                    close(clients[i].sock);
                }
                printf("\n%lld ", get_time_ms());
                printf("Received heartbeat from client: %d\n", recv_data[0]);
                recv_time = get_time_ms();
            } else {
                printf("Cycle client: fd not set\n");
            }
        }
    }
}

int create_udp_send_socket() {
    udp_send_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return 0;
}

void send_message(int client_id) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT_OUT);
    //addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_aton(inet_ntoa(clients[client_id].addr), &(addr.sin_addr));

    printf("\n%lld ", get_time_ms());
    printf("Send heartbeat to client: %d", send_data[0]);
    sendto(udp_send_sock, (char*)send_data, SEND_DATA_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr));
}

int main(int argc, char *argv[])
{
    //QCoreApplication a(argc, argv);

    tcp_recv_sock = create_tcp_lsn_socket(TCP_PORT_IN);

    int client_id = wait_client(tcp_recv_sock);

    int recv_sock = create_udp_recv_socket(client_id);
    if (recv_sock <= 0) {
        perror("Unable to create UDP receive socket");
        return 1;
    }

    clients[client_id].sock = recv_sock;

    if (create_udp_send_socket() != 0) {
        perror("Unable to create UDP send socket");
        return 1;
    }

    int last_send_time = 0;
    while(1) {
        get_message();

        long long curr_time = get_time_ms();

        if (recv_time != 0) {
            if (curr_time - recv_time > POLL_PERIOD * 2) {
                printf("Client send delay\n");
            }
            if (curr_time - recv_time > POLL_PERIOD * 4) {
                printf("Client connection timeout\n");
                break;
            }
        }

        if (get_time_ms() - last_send_time > POLL_PERIOD) {
            for (int i = 0; i < MAX_CLIENTS; i++) {
                send_message(i);
            }
        }

        last_send_time = get_time_ms();
        send_data[0] = send_data[0] + 1;
        msleep(1);
    }

    close(udp_recv_sock);
    close(udp_send_sock);
    close(tcp_recv_sock);

    return 0; // a.exec();
}
