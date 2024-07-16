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

#define RECV_DATA_SIZE 256
#define SEND_DATA_SIZE 159

const char *server_ip = "192.168.0.210";

#define TCP_PORT_OUT 44818

#define UDP_PORT_OUT 2222
#define UDP_PORT_IN  2223

#define POLL_PERIOD 20

int tcp_send_sock = 0;

int udp_recv_sock = 0;
int udp_send_sock = 0;

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

int connect_to_server() {
    tcp_send_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT_OUT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_aton(server_ip, &(addr.sin_addr));

    return connect(tcp_send_sock, (struct sockaddr *)&addr, sizeof(addr));
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
    addr.sin_port = htons(UDP_PORT_IN);
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
            addr.sin_port = htons(UDP_PORT_IN);
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
    //QCoreApplication a(argc, argv);

    signal(SIGPIPE, SIG_IGN);

    if (connect_to_server() != 0) {
        perror("Unable to connect TCP");
        exit(1);
    }

    sleep(2);

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

        msleep(POLL_PERIOD);
    }

    return 0;
}
