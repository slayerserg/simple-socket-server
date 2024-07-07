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
#include <chrono>
#include <stdlib.h>
#include <poll.h>

#define RECV_DATA_SIZE 256
#define SEND_DATA_SIZE 205
#define MAX_CONN 2

#define TCP_PORT_IN 44818

#define UDP_PORT_IN  2222
//#define UDP_PORT_OUT 2223

int udp_port_out = 2223;

int poll_period = 50;
bool tcms_client = false;

int tcp_recv_sock = 0;

int udp_recv_sock = 0;
int udp_send_sock = 0;

fd_set readfds;
int max_fd = 0;

long recv_time = 0;
long last_send_time = 0;

bool client_accepted = false;

typedef struct _Client
{
    int sock;
    in_addr addr;
} Client;

Client clients;

struct pollfd pollfds[MAX_CONN];

unsigned char recv_data[RECV_DATA_SIZE] = {};

unsigned char send_data[SEND_DATA_SIZE] = { \
0x02, 0x00, 0x02, 0x80, 0x08, 0x00, 0x4e, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xb1, 0x00, \
0xbb, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x2c, 0xbb, 0xe7, 0xc0, 0x2c, 0x3b, 0x8e, \
0x22, 0x4b, 0x9d, 0x9e, 0xcf, 0x1a, 0xc5, 0x39, 0x7c, 0x9f, 0xde, 0xf8, 0x32, 0x0e, 0x9b, 0x7a, \
0x09, 0x5d, 0xf5, 0xb3, 0x36, 0xc8, 0xa4, 0x10, 0x3a, 0xd3, 0xe2, 0x89, 0x63, 0xec, 0xec, 0xd9, \
0xbb, 0x05, 0x4d, 0x46, 0xb5, 0x97, 0xfe, 0x47, 0xa7, 0xc9, 0x8b, 0x2c, 0x80, 0x97, 0x94, 0x90, \
0xbc, 0xeb, 0x1b, 0x7b, 0xdd, 0xb9, 0xef, 0xd2, 0xda, 0x87, 0xe3, 0x56, 0x1f, 0xf5, 0x03, 0x17, \
0x82, 0xc9, 0x46, 0x99, 0x0b, 0x6d, 0xbb, 0xb7, 0x85, 0x00, 0x15, 0xc8, 0xfb, 0xf6, 0x3a, 0x38, \
0x58, 0xe4, 0x09, 0x4c, 0x7f, 0xea, 0x15, 0x78, 0x43, 0x34, 0x44, 0x67, 0x5e, 0xee, 0x4e, 0xab, \
0xe4, 0x3e, 0xd4, 0xb4, 0xcc, 0x9b, 0xdb, 0xdf, 0xd8, 0x65, 0x81, 0xcd, 0x18, 0x18, 0x55, 0xa4, \
0xb9, 0x5e, 0x22, 0x05, 0x54, 0x07, 0xba, 0x0b, 0x27, 0x1f, 0xdb, 0xf1, 0x5f, 0xab, 0x9b, 0x7f, \
0xd6, 0xe8, 0x92, 0xc1, 0xca, 0x27, 0xa4, 0x7f, 0x46, 0x3d, 0x25, 0xdb, 0x4a, 0xd7, 0x57, 0x64, \
0xa6, 0x65, 0x1a, 0xcb, 0xa1, 0xe4, 0x80, 0x70, 0x6c, 0xb0, 0x43, 0x98, 0xea, 0xc3, 0x1f, 0xf8, \
0x9a, 0xf6, 0x70, 0xa2, 0xc8, 0xb1, 0x67, 0xf1, 0x77, 0x69, 0x5f, 0x7d, 0x20};

long long get_time_ms () {
  struct timespec t ;
  clock_gettime ( CLOCK_REALTIME , & t ) ;
  return t.tv_sec * 1000 + ( t.tv_nsec + 500000 ) / 1000000 ;
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
        return -1;
    }

    int listenResult = listen(server_socket, 5);
    if (listenResult == -1)
    {
        perror("TCP listener listenResult");
        return -1;
    }
    printf("TCP listener started\n\n");
    return server_socket;
}

uint16_t get_int_from_message(const unsigned char **const buffer) {
  const unsigned char *const buffer_address = (unsigned char *) *buffer;
  uint16_t data = buffer_address[0] | buffer_address[1] << 8;
  *buffer += 2;
  return data;
}

#define REPLY_1_SIZE 28
#define REPLY_2_SIZE 54
#define REPLY_3_SIZE 229
#define REPLY_4_SIZE 203
#define REPLY_5_SIZE 90

unsigned char reply_1[28] = {0x65, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};

unsigned char reply_2[54] = {0x6f, 0x00, 0x1e, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 
0x00, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x0e, 0x00, 0x8e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

unsigned char reply_3[229] = {0x6f, 0x00, 0xcd, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 
0x00, 0x00, 0x00, 0x00, 0xb2, 0x00, 0xbd, 0x00, 0x8e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 
0x02, 0x2c, 0xbb, 0xe7, 0xc0, 0x2c, 0x3b, 0x8e, 0x22, 0x4b, 0x9d, 0x9e, 0xcf, 0x1a, 0xc5, 0x39, 
0x7c, 0x9f, 0xde, 0xf8, 0x32, 0x0e, 0x9b, 0x7a, 0x09, 0x5d, 0xf5, 0xb3, 0x36, 0xc8, 0xa4, 0x10, 
0x3a, 0xd3, 0xe2, 0x89, 0x63, 0xec, 0xec, 0xd9, 0xbb, 0x05, 0x4d, 0x46, 0xb5, 0x97, 0xfe, 0x47, 
0xa7, 0xc9, 0x8b, 0x2c, 0x80, 0x97, 0x94, 0x90, 0xbc, 0xeb, 0x1b, 0x7b, 0xdd, 0xb9, 0xef, 0xd2, 
0xda, 0x87, 0xe3, 0x56, 0x1f, 0xf5, 0x03, 0x17, 0x82, 0xc9, 0x46, 0x99, 0x0b, 0x6d, 0xbb, 0xb7, 
0x85, 0x00, 0x15, 0xc8, 0xfb, 0xf6, 0x3a, 0x38, 0x58, 0xe4, 0x09, 0x4c, 0x7f, 0xea, 0x15, 0x78, 
0x43, 0x34, 0x44, 0x67, 0x5e, 0xee, 0x4e, 0xab, 0xe4, 0x3e, 0xd4, 0xb4, 0xcc, 0x9b, 0xdb, 0xdf, 
0xd8, 0x65, 0x81, 0xcd, 0x18, 0x18, 0x55, 0xa4, 0xb9, 0x5e, 0x22, 0x05, 0x54, 0x07, 0xba, 0x0b, 
0x27, 0x1f, 0xdb, 0xf1, 0x5f, 0xab, 0x9b, 0x7f, 0xd6, 0xe8, 0x92, 0xc1, 0xca, 0x27, 0xa4, 0x7f, 
0x46, 0x3d, 0x25, 0xdb, 0x4a, 0xd7, 0x57, 0x64, 0xa6, 0x65, 0x1a, 0xcb, 0xa1, 0xe4, 0x80, 0x70, 
0x6c, 0xb0, 0x43, 0x98, 0xea, 0xc3, 0x1f, 0xf8, 0x9a, 0xf6, 0x70, 0xa2, 0xc8, 0xb1, 0x67, 0xf1, 
0x77, 0x69, 0x5f, 0x7d, 0x20};

unsigned char reply_4[203] = {0x6f, 0x00, 0xb3, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 
0x00, 0x00, 0x00, 0x00, 0xb2, 0x00, 0xa3, 0x00, 0x8e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

unsigned char reply_5[90] = {0x6f, 0x00, 0x42, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 
0x00, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x1e, 0x00, 0xd4, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 
0x4e, 0x01, 0x00, 0x00, 0xbc, 0x01, 0xda, 0xfa, 0x0d, 0xf0, 0xad, 0x8b, 0xa0, 0x86, 0x01, 0x00, 
0xa0, 0x86, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0x10, 0x00, 0x00, 0x02, 0x08, 0xae, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

int wait_client(int tcp_recv_sock) {
    int reply_num = 1;

    pollfds[0].fd = tcp_recv_sock;
    pollfds[0].events = POLLIN | POLLPRI;

    while(1) {
        // FD_ZERO(&readfds);
        // FD_SET(tcp_recv_sock, &readfds);
        // for (int i = 0; i < MAX_CLIENTS; i++) {
        //     if (clients[i].sock != 0) {
        //         FD_SET(clients[i].sock, &readfds);
        //     }
        // }

        //int selectResult = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        int pollResult = poll(pollfds, 2, 5000);

        if (pollResult > 0) {
            if (pollfds[0].revents & POLLIN) {
                if (clients.sock == 0)
                {
                    struct sockaddr_in cliaddr;
                    socklen_t addrlen = sizeof(cliaddr);
                    clients.sock = accept(tcp_recv_sock, (struct sockaddr *)&cliaddr, &addrlen);
                    clients.addr = cliaddr.sin_addr;
                    printf("Accept success, IP => %s, sock = %d\n", inet_ntoa(cliaddr.sin_addr), clients.sock);
                    client_accepted = true;
                    pollfds[1].fd = clients.sock;
                    pollfds[1].events = POLLIN | POLLPRI;
                    break;
                    //return i;
                }
            }
            if (tcms_client) {
                if (clients.sock != 0 && pollfds[1].revents & POLLIN) {
                    long data_len = 512;
                    unsigned char incoming_message[data_len] = {0};


                    long number_of_read_bytes = recv(clients.sock, incoming_message, data_len, 0);
                    if (number_of_read_bytes > 0) {
                        printf("Client data received len = %ld\n", number_of_read_bytes);
                    }
                    
                    if (reply_num == 1 && number_of_read_bytes == 28) {
                        print_buff(incoming_message, number_of_read_bytes);
                        printf("Client data send len = %ld\n\n", send(clients.sock, (char*)reply_1, REPLY_1_SIZE, 0));
                        reply_num++;
                    }
                    else if (reply_num == 2 && number_of_read_bytes == 48) {
                        print_buff(incoming_message, number_of_read_bytes);
                        printf("Client data send len = %ld\n\n", send(clients.sock, (char*)reply_2, REPLY_2_SIZE, 0));
                        reply_num++;
                    }
                    else if (reply_num == 3 && number_of_read_bytes == 48) {
                        print_buff(incoming_message, number_of_read_bytes);
                        printf("Client data send len = %ld\n\n", send(clients.sock, (char*)reply_3, REPLY_3_SIZE, 0));
                        reply_num++;
                    }
                    else if (reply_num == 4 && number_of_read_bytes == 48) {
                        print_buff(incoming_message, number_of_read_bytes);
                        printf("Client data send len = %ld\n\n", send(clients.sock, (char*)reply_4, REPLY_4_SIZE, 0));
                        reply_num++;
                    }
                    else if (reply_num == 5 && number_of_read_bytes == 90) {
                        print_buff(incoming_message, number_of_read_bytes);
                        printf("Client data send len = %ld\n\n", send(clients.sock, (char*)reply_5, REPLY_5_SIZE, 0));
                        reply_num++;
                        return 0;
                    }
                }
            } else {
                return 0;
            }
        }
        msleep(5);  
    }
    return 0;
}

void create_udp_recv_socket()
{
    udp_recv_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT_IN);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //inet_aton(inet_ntoa(clients[client_id].addr), &(addr.sin_addr));

    int bindResult = bind(udp_recv_sock, (struct sockaddr *)&addr, sizeof(addr));
    if (bindResult == -1)
    {
        perror("UDP receiver bindResult");
        return;
    }

    pollfds[1].fd = udp_recv_sock;
    pollfds[1].events = POLLIN | POLLPRI;

    printf("UDP receiver started\n");
}

void get_message()
{
    struct sockaddr_in addr;

    int pollResult = poll(pollfds, 2, 5000);

    if (pollResult > 0) {
        printf("Revents server = %d\n", (pollfds[0].revents & POLLIN));
        printf("Revents client tcp = %d\n", (pollfds[1].revents & POLLIN));
        //printf("Revents client udp = %d\n", (pollfds[2].revents & POLLIN));
        if (pollfds[1].revents & POLLIN)
        {
            addr.sin_family = AF_INET;
            addr.sin_port = htons(UDP_PORT_IN);
            inet_aton(inet_ntoa(clients.addr), &(addr.sin_addr));
            socklen_t addr_size = sizeof(addr);

            int size = recvfrom(udp_recv_sock, recv_data, RECV_DATA_SIZE, 0, (struct sockaddr *)&addr, &addr_size);
            if (size == -1 || size == 0)
            {
                printf("Closing UDP receiver\n");
                udp_recv_sock = 0;
                close(udp_recv_sock);
            } else {
                printf("\n%lld ", get_time_ms());
                printf("Received heartbeat from client: %d\n", recv_data[24]);
                //print_buff(recv_data, size);
                recv_time = get_time_ms();
            }
        } else {
            //printf("Cycle client: fd not set\n");
        }
    }
}

int create_udp_send_socket() {
    udp_send_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    int option_value = 1;
    setsockopt(udp_send_sock, SOL_SOCKET, SO_REUSEADDR,
               (char *) &option_value,
               sizeof(option_value));

    //uint16_t set_tos = 16;
    //setsockopt(udp_send_sock, IPPROTO_IP, IP_TOS, &set_tos, sizeof(set_tos));
    
    return 0;
}

void send_message() {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(udp_port_out);
    inet_aton(inet_ntoa(clients.addr), &(addr.sin_addr));

    printf("\n%lld ", get_time_ms());
    printf("Send heartbeat to client: %d\n", send_data[24]);
    //print_buff((unsigned char*)send_data, SEND_DATA_SIZE);
    sendto(udp_send_sock, (char*)send_data, SEND_DATA_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr));
    last_send_time = get_time_ms();
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: \n");
        printf("    ./select-server <poll_period> <tcms_client>\n");
        printf("\n");
        printf("Example: \n");
        printf("    ./select-server 50 true\n");
        return 0;
    }

    poll_period = atoi(argv[1]);
    if (!strcmp(argv[2], "true")) {
        tcms_client = true;
    }

    if (tcms_client) {
        udp_port_out = 2222;
    }

    printf("Starting server (select poll method) with settings:\n");
    printf("    - poll_period = %d\n", poll_period);
    printf("    - tcms_client = %s\n", (tcms_client > 0) ? "true" : "false");
    printf("\n");

    tcp_recv_sock = create_tcp_lsn_socket(TCP_PORT_IN);
    if (tcp_recv_sock <= 0) {
        perror("Unable to create TCP listener socket");
        return 1;
    }

    wait_client(tcp_recv_sock);

    create_udp_recv_socket();
    if (udp_recv_sock <= 0) {
        perror("Unable to create UDP receive socket");
        return 1;
    }

    if (create_udp_send_socket() != 0) {
        perror("Unable to create UDP send socket");
        return 1;
    }

    while(1) {
        get_message();

        long curr_time = get_time_ms();

        if (recv_time != 0) {
            if (curr_time - recv_time > poll_period * 2) {
                printf("%lld Client send delay = %ld\n", get_time_ms(), curr_time - recv_time);
            }
            if (curr_time - recv_time > poll_period * 5) {
                printf("%lld Client connection timeout = %ld\n", get_time_ms(), curr_time - recv_time);
                //break;
            }
            if (curr_time - recv_time > poll_period * 200) {
                printf("%lld Client connection stopped = %ld\n", get_time_ms(), curr_time - recv_time);
                break;
            }
        }

        if (get_time_ms() - last_send_time >= poll_period - 1) {
            //for (int i = 0; i < MAX_CLIENTS; i++) {
                send_message();
                send_data[10] = send_data[10] + 1;
                send_data[18] = send_data[18] + 1;
                send_data[24] = send_data[24] + 1;
            //}
        }
        
        msleep(5);
    }

    close(udp_recv_sock);
    close(udp_send_sock);
    close(tcp_recv_sock);

    return 0;
}
