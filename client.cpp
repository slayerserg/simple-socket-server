#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>

#define RECV_DATA_SIZE 256
#define SEND_DATA_SIZE 159

char *server_ip = "192.168.0.210";

#define TCP_PORT_OUT 44818

#define UDP_PORT_OUT  2222
int udp_port_in = 2223;

int poll_period = 20;

int tcp_send_sock = 0;

int udp_recv_sock = 0;
int udp_send_sock = 0;

bool tcms_client = false;

fd_set readfds;
int max_fd = 0;

int last_hb = 0;
int unhandled = 0;
int total_unhandled = 0;
unsigned long long total_sent = 0;
unsigned long long total_received = 0;

unsigned char recv_data[RECV_DATA_SIZE] = {};
unsigned char send_data[SEND_DATA_SIZE] = \
  {0x01, 0x09, 0x2C, 0x9A, 0x63, 0x36, 0xFF, 0xFF, 0x33, 0x32, 0x31, 0x5A, 0x78, 0x03, 0xFF, 0x00, \
   0x46, 0x52, 0x6C, 0x4F, 0x67, 0x49, 0x6E, 0x31, 0x32, 0x33, 0x21, 0x00, 0x50, 0x77, 0x64, 0x31, \
   0x32, 0x33, 0x24, 0x00, 0x00, 0x00, 0xC0, 0x04, 0x00, 0x20, 0xF1, 0x47, 0x02, 0x07, 0x03, 0x02, \
   0x45, 0x4D, 0x55, 0x2D, 0x74, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, 0x49, 0x64, 0x31, 0x32, \
   0x33, 0x34, 0x35, 0x36, 0x23, 0x00, 0x00, 0x00, 0x02, 0x00, 0x78, 0x3D, 0x31, 0x2D, 0x4E, 0x62, \
   0x55, 0x6E, 0x69, 0x74, 0x00, 0x00, 0x63, 0x61, 0x72, 0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, \
   0x00, 0x00, 0x00, 0x20, 0xF1, 0x47, 0x00, 0x20, 0xF1, 0x47, 0x80, 0x00, 0x1E, 0x19, 0x07, 0x00, \
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

long long get_time_ms () {
  struct timespec t ;
  clock_gettime ( CLOCK_REALTIME , & t ) ;
  return t.tv_sec * 1000 + ( t.tv_nsec + 500000 ) / 1000000 ;
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

#define SRV_REPLY_1_SIZE 28
#define SRV_REPLY_2_SIZE 54
#define SRV_REPLY_3_SIZE 229
#define SRV_REPLY_4_SIZE 203
#define SRV_REPLY_5_SIZE 90

#define REPLY_1_SIZE 28
unsigned char reply_1[REPLY_1_SIZE] = \
{ 0x65, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00
};

#define REPLY_2_SIZE 48
unsigned char reply_2[REPLY_2_SIZE] = \
{ 0x6f, 0x00, 0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, \
  0x00, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x08, 0x00, 0x0e, 0x03, 0x20, 0x04, 0x24, 0x01, 0x30, 0x03
};

#define REPLY_3_SIZE 48
unsigned char reply_3[REPLY_3_SIZE] = \
{ 0x6f, 0x00, 0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, \
  0x00, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x08, 0x00, 0x0e, 0x03, 0x20, 0x04, 0x24, 0x64, 0x30, 0x03
};

#define REPLY_4_SIZE 48
unsigned char reply_4[REPLY_4_SIZE] = \
{ 0x6f, 0x00, 0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, \
  0x00, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x08, 0x00, 0x0e, 0x03, 0x20, 0x04, 0x24, 0x65, 0x30, 0x03
};

#define REPLY_5_SIZE 90
unsigned char reply_5[REPLY_5_SIZE] = \
{ 0x6f, 0x00, 0x42, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, \
  0x00, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x32, 0x00, 0x54, 0x02, 0x20, 0x06, 0x24, 0x01, 0x0a, 0x0a, \
  0x4d, 0x01, 0x00, 0x00, 0x4e, 0x01, 0x00, 0x00, 0xbc, 0x01, 0xda, 0xfa, 0x0d, 0xf0, 0xad, 0x8b, \
  0x00, 0x00, 0x00, 0x00, 0x20, 0x4e, 0x00, 0x00, 0xa5, 0x46, 0x20, 0x4e, 0x00, 0x00, 0xbb, 0x40, \
  0x01, 0x04, 0x20, 0x04, 0x24, 0x01, 0x2c, 0x65, 0x2c, 0x64
};


int connect_to_server() {
    tcp_send_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT_OUT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_aton(server_ip, &(addr.sin_addr));

    int res = connect(tcp_send_sock, (struct sockaddr *)&addr, sizeof(addr));

    if (res == 0 && tcms_client) {
        int reply_num = 1;

        struct pollfd pollfds[2] = {0};

        pollfds[0].fd = tcp_send_sock;
        pollfds[0].events = POLLIN | POLLPRI;

        send(tcp_send_sock, (char*)reply_1, REPLY_1_SIZE, 0);

        while(1) {
            int pollResult = poll(pollfds, 2, 5000);

            if (pollResult > 0 && pollfds[0].revents & POLLIN) {
                long data_len = 512;
                unsigned char incoming_message[data_len] = {0};


                long number_of_read_bytes = recv(tcp_send_sock, incoming_message, data_len, 0);
                if (number_of_read_bytes > 0) {
                    printf("Received from server data len = %ld\n", number_of_read_bytes);
                }
                
                if (reply_num == 1 && number_of_read_bytes == SRV_REPLY_1_SIZE) {
                    print_buff(incoming_message, number_of_read_bytes);
                    printf("Send to server data len = %ld\n\n", send(tcp_send_sock, (char*)reply_2, REPLY_2_SIZE, 0));
                    reply_num++;
                }
                else if (reply_num == 2 && number_of_read_bytes == SRV_REPLY_2_SIZE) {
                    print_buff(incoming_message, number_of_read_bytes);
                    printf("Send to server data len = %ld\n\n", send(tcp_send_sock, (char*)reply_3, REPLY_3_SIZE, 0));
                    reply_num++;
                }
                else if (reply_num == 3 && number_of_read_bytes == SRV_REPLY_3_SIZE) {
                    print_buff(incoming_message, number_of_read_bytes);
                    printf("Send to server data len = %ld\n\n", send(tcp_send_sock, (char*)reply_4, REPLY_4_SIZE, 0));
                    reply_num++;
                }
                else if (reply_num == 4 && number_of_read_bytes == SRV_REPLY_4_SIZE) {
                    print_buff(incoming_message, number_of_read_bytes);
                    printf("Send to server data len = %ld\n\n", send(tcp_send_sock, (char*)reply_5, REPLY_5_SIZE, 0));
                    reply_num++;
                }
                else if (reply_num == 5 && number_of_read_bytes == SRV_REPLY_5_SIZE) {
                    print_buff(incoming_message, number_of_read_bytes);
                    reply_num++;
                    return 0;
                }
            }
        }
        
    }
    return res;
}

int create_udp_send_socket() {
    udp_send_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return 0;
}

void send_message() {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT_OUT);
    inet_pton(AF_INET, server_ip, &(addr.sin_addr));

    sendto(udp_send_sock, (char*)send_data, SEND_DATA_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr));
    total_sent++;
}

int create_udp_recv_socket()
{
    udp_recv_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(udp_port_in);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

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

    FD_SET(udp_recv_sock, &readfds);
    max_fd = udp_recv_sock;

    struct timeval g_time_value;
    g_time_value.tv_sec = 0;
    g_time_value.tv_usec = 10000;

    int select_result = select(max_fd + 1, &readfds, 0, 0, &g_time_value);

    if (select_result > 0) {
        if (FD_ISSET(udp_recv_sock, &readfds))
        {
            addr.sin_family = AF_INET;
            addr.sin_port = htons(udp_port_in);
            inet_aton(server_ip, &(addr.sin_addr));
            socklen_t addr_size = sizeof(addr);

            int size = recvfrom(udp_recv_sock, recv_data, RECV_DATA_SIZE, 0, (struct sockaddr *)&addr, &addr_size);
            if (size == -1 || size == 0)
            {
                printf("Closing UDP receiver\n");
                udp_recv_sock = 0;
                close(udp_recv_sock);
            }
            total_received++;
        } else {
            printf("Cycle client: fd not set\n");
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: \n");
        printf("    ./client <server> <poll_period> <work_as_tcms>\n");
        printf("\n");
        printf("Example: \n");
        printf("    ./client 192.168.1.1 50 true\n");
        return 0;
    }

    server_ip = argv[1];
    poll_period = atoi(argv[2]);

    if (!strcmp(argv[3], "true")) {
        tcms_client = true;
    }

    if (tcms_client) {
        udp_port_in = 2222;
    }

    signal(SIGPIPE, SIG_IGN);

    if (connect_to_server() != 0) {
        perror("Unable to connect TCP");
        exit(1);
    }

    if (create_udp_send_socket() != 0) {
        perror("Unable to create send UDP sock");
        exit(1);
    }

   if (create_udp_recv_socket() <= 0) {
       perror("Unable to create receive UDP sock");
       exit(1);
   }

    while (1)
    {
        printf("\n%lld\n", get_time_ms());
        printf("Send heartbeat to server: %d\n", send_data[24]);
        printf("OUTPUTS:\n");
        print_buff(send_data, SEND_DATA_SIZE);
        send_message();
        send_data[24] = send_data[24] + 1;


        get_message();
        printf("\n%lld\n", get_time_ms());
        printf("INPUTS:\n");
        print_buff(recv_data, RECV_DATA_SIZE);

        if (last_hb == recv_data[24]) {
            total_unhandled++;
            unhandled++;
        } else {
            unhandled = 0;
            last_hb = recv_data[24];
        }
        
        printf("\n%lld\n", get_time_ms());
        printf("Received heartbeat from server: %d\n", recv_data[24]);
        printf("Total sent = %llu, total received = %llu, current unhandled = %d, total unhandled = %d\n", total_sent, total_received, unhandled, total_unhandled);

        if (unhandled > 4) {
            printf("Current unhandled = %d, STOP\n", unhandled);
            break;
        }

        msleep(poll_period);
    }

    return 0;
}
