#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>

#define MAX_EVENTS 64
#define MESSAGE_SIZE 16
#define DEFAULT_CLIENT_THREADS 4
#define TIMEOUT_US 100000 // 100ms timeout

char *server_ip = "127.0.0.1";
int server_port = 12345;
int num_client_threads = DEFAULT_CLIENT_THREADS;
int num_requests = 1000000;

typedef struct {
    int epoll_fd;
    int socket_fd;
    long long total_rtt;
    long tx_cnt;        // Packets sent counter
    long rx_cnt;        // Packets received counter
    long lost_pkt_cnt;  // Lost packets counter
    float request_rate;
    struct sockaddr_in server_addr;
} client_thread_data_t;

void *client_thread_func(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    struct epoll_event event, events[MAX_EVENTS];
    char send_buf[MESSAGE_SIZE] = "ABCDEFGHIJKMLNOP";
    char recv_buf[MESSAGE_SIZE];
    struct timeval start, end, timeout;
    int seq_num = 0;
    socklen_t addr_len = sizeof(data->server_addr);

    // Register socket with epoll
    event.events = EPOLLIN;
    event.data.fd = data->socket_fd;
    if (epoll_ctl(data->epoll_fd, EPOLL_CTL_ADD, data->socket_fd, &event) == -1) {
        perror("epoll_ctl: client socket");
        return NULL;
    }

    while (data->tx_cnt < num_requests) {
        // Add sequence number to packet
        memcpy(send_buf, &seq_num, sizeof(int));
        
        gettimeofday(&start, NULL);
        data->tx_cnt++;

        // Send packet to server
        if (sendto(data->socket_fd, send_buf, MESSAGE_SIZE, 0,
                  (struct sockaddr *)&data->server_addr, addr_len) == -1) {
            perror("sendto");
            continue;
        }

        // Wait for ACK with timeout
        int received = 0;
        while (!received) {
            timeout.tv_sec = 0;
            timeout.tv_usec = TIMEOUT_US;
            
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(data->socket_fd, &read_fds);
            
            int ret = select(data->socket_fd + 1, &read_fds, NULL, NULL, &timeout);
            if (ret == -1) {
                perror("select");
                break;
            } else if (ret == 0) {
                // Timeout - resend packet
                if (sendto(data->socket_fd, send_buf, MESSAGE_SIZE, 0,
                          (struct sockaddr *)&data->server_addr, addr_len) == -1) {
                    perror("sendto (retry)");
                }
                data->tx_cnt++; // Count retransmission
                continue;
            }

            // Packet received
            if (recvfrom(data->socket_fd, recv_buf, MESSAGE_SIZE, 0,
                       (struct sockaddr *)&data->server_addr, &addr_len) == -1) {
                perror("recvfrom");
                continue;
            }

            // Check if ACK matches our sequence number
            int ack_num;
            memcpy(&ack_num, recv_buf, sizeof(int));
            if (ack_num == seq_num) {
                received = 1;
                data->rx_cnt++;
                gettimeofday(&end, NULL);
                long long rtt = (end.tv_sec - start.tv_sec) * 1000000LL + 
                               (end.tv_usec - start.tv_usec);
                data->total_rtt += rtt;
            }
        }
        seq_num = !seq_num; // Toggle sequence number (0/1)
    }

    // Calculate lost packets
    data->lost_pkt_cnt = data->tx_cnt - data->rx_cnt;
    
    // Calculate request rate
    if (data->total_rtt > 0) {
        data->request_rate = (float)data->rx_cnt / (data->total_rtt / 1000000.0);
    }

    return NULL;
}

void run_client() {
    pthread_t threads[num_client_threads];
    client_thread_data_t thread_data[num_client_threads];
    struct sockaddr_in server_addr;

    for (int i = 0; i < num_client_threads; i++) {
        // Create UDP socket
        thread_data[i].socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (thread_data[i].socket_fd == -1) {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        // Set up server address
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(server_port);
        memcpy(&thread_data[i].server_addr, &server_addr, sizeof(server_addr));

        // Create epoll instance
        thread_data[i].epoll_fd = epoll_create1(0);
        if (thread_data[i].epoll_fd == -1) {
            perror("epoll_create1");
            close(thread_data[i].socket_fd);
            exit(EXIT_FAILURE);
        }

        // Initialize thread data
        thread_data[i].total_rtt = 0;
        thread_data[i].tx_cnt = 0;
        thread_data[i].rx_cnt = 0;
        thread_data[i].lost_pkt_cnt = 0;
        thread_data[i].request_rate = 0.0;

        // Create client thread
        if (pthread_create(&threads[i], NULL, client_thread_func, &thread_data[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    long long total_rtt = 0;
    long total_tx = 0;
    long total_rx = 0;
    long total_lost = 0;
    float total_request_rate = 0.0;

    for (int i = 0; i < num_client_threads; i++) {
        pthread_join(threads[i], NULL);
        total_rtt += thread_data[i].total_rtt;
        total_tx += thread_data[i].tx_cnt;
        total_rx += thread_data[i].rx_cnt;
        total_lost += thread_data[i].lost_pkt_cnt;
        total_request_rate += thread_data[i].request_rate;
        
        printf("Thread %d: Sent %ld, Received %ld, Lost %ld (%.2f%%)\n", 
               i, thread_data[i].tx_cnt, thread_data[i].rx_cnt, thread_data[i].lost_pkt_cnt,
               (thread_data[i].lost_pkt_cnt * 100.0) / thread_data[i].tx_cnt);
        
        close(thread_data[i].socket_fd);
        close(thread_data[i].epoll_fd);
    }

    printf("\nAggregate Statistics:\n");
    printf("Total Packets Sent: %ld\n", total_tx);
    printf("Total Packets Received: %ld\n", total_rx);
    printf("Total Packets Lost: %ld (%.2f%%)\n", total_lost, (total_lost * 100.0) / total_tx);
    printf("Average RTT: %lld us\n", total_rtt / (total_rx > 0 ? total_rx : 1));
    printf("Total Request Rate: %f messages/s\n", total_request_rate);
}

void run_server() {
    int sock_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buf[MESSAGE_SIZE];
    long packet_count = 0;
    struct timeval start_time, current_time;
    
    // Create UDP socket
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);

    // Bind socket to address
    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    gettimeofday(&start_time, NULL);
    printf("[SERVER] Started on %s:%d at %s", 
           server_ip, server_port, ctime(&start_time.tv_sec));
    printf("[SERVER] Ready to accept UDP packets...\n");

    while (1) {
        // Receive packet from client
        int n = recvfrom(sock_fd, buf, MESSAGE_SIZE, 0,
                        (struct sockaddr *)&client_addr, &client_len);
        if (n == -1) {
            perror("[SERVER] recvfrom error");
            continue;
        }

        packet_count++;
        
        // Log first packet from new clients
        if (packet_count % 1000 == 0) {
            gettimeofday(&current_time, NULL);
            long elapsed = (current_time.tv_sec - start_time.tv_sec);
            printf("[SERVER] Received %ld packets (%.2f pkt/sec) from %s:%d\n",
                   packet_count,
                   (float)packet_count/elapsed,
                   inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port));
        }

        // Get sequence number from packet
        int received_seq;
        memcpy(&received_seq, buf, sizeof(int));

        // Simulate processing delay when overloaded
        if (packet_count % 500 == 0) {
            usleep(1000); // 1ms delay every 500 packets
        }

        // Send ACK back to client
        if (sendto(sock_fd, &received_seq, sizeof(int), 0,
                 (struct sockaddr *)&client_addr, client_len) == -1) {
            perror("[SERVER] sendto failed");
            
            // Log send errors which indicate potential congestion
            gettimeofday(&current_time, NULL);
            fprintf(stderr, "[SERVER] [ERROR] Failed to send ACK at %s", 
                    ctime(&current_time.tv_sec));
        }
    }

    close(sock_fd);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "server") == 0) {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);
        run_server();
    } else if (argc > 1 && strcmp(argv[1], "client") == 0) {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);
        if (argc > 4) num_client_threads = atoi(argv[4]);
        if (argc > 5) num_requests = atoi(argv[5]);
        run_client();
    } else {
        printf("Usage: %s <server|client> [server_ip server_port num_client_threads num_requests]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    return 0;
}
