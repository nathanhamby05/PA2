#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>

#define MAX_EVENTS 64
#define MAX_PAYLOAD_SIZE 12
#define DEFAULT_CLIENT_THREADS 4
#define MAX_CLIENTS 10000          // Increased from 1000 to handle more clients
#define TIMEOUT_US 100000          // 100ms timeout
#define MAX_RETRIES 5              // Maximum retransmission attempts
#define MAX_SEQ 1                  // For stop-and-wait protocol

// Global configuration
char *server_ip = "127.0.0.1";
int server_port = 12345;
int num_client_threads = DEFAULT_CLIENT_THREADS;
int num_requests = 1000;

typedef enum {
    FRAME_ARRIVAL,
    TIMEOUT,
    NETWORK_LAYER_READY
} event_type;

typedef struct {
    uint8_t data[MAX_PAYLOAD_SIZE];
} packet;

typedef struct {
    uint32_t client_id;
    uint32_t seq;       // Sequence number (0 or 1)
    uint32_t ack;       // Acknowledgment number
    packet info;        // Network layer packet
} frame;

typedef struct {
    int epoll_fd;
    int socket_fd;
    uint32_t client_id;
    long long total_rtt;
    long tx_cnt;
    long rx_cnt;
    long retransmit_cnt;
    float request_rate;
    struct sockaddr_in server_addr;
} client_thread_data_t;

// Simulate network layer
void from_network_layer(packet *p) {
    static const char *data = "ABCDEFGHIJKL";
    memcpy(p->data, data, MAX_PAYLOAD_SIZE);
}

// Physical layer transmission
int to_physical_layer(int fd, frame *f, struct sockaddr_in *addr) {
    return sendto(fd, f, sizeof(frame), 0, 
                 (struct sockaddr *)addr, sizeof(*addr));
}

// Physical layer reception
int from_physical_layer(int fd, frame *f, struct sockaddr_in *addr) {
    socklen_t len = sizeof(*addr);
    return recvfrom(fd, f, sizeof(frame), 0, 
                   (struct sockaddr *)addr, &len);
}

event_type wait_for_event(int fd, int timeout_us) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    
    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = timeout_us
    };
    
    int ret = select(fd + 1, &fds, NULL, NULL, &tv);
    if (ret == 0) return TIMEOUT;
    if (ret > 0) return FRAME_ARRIVAL;
    return TIMEOUT; // Treat errors as timeout
}

void *sender_thread(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    frame s, r;
    packet buffer;
    uint32_t next_frame_to_send = 0;
    struct timeval start, end;
    
    while (data->tx_cnt < num_requests) {
        from_network_layer(&buffer);
        
        // Prepare frame
        s.client_id = data->client_id;
        s.seq = next_frame_to_send;
        s.ack = 0;
        s.info = buffer;
        
        int retries = 0;
        int acked = 0;
        
        gettimeofday(&start, NULL);
        
        while (!acked && retries < MAX_RETRIES) {
            if (retries == 0) {
                data->tx_cnt++;
            } else {
                data->retransmit_cnt++;
            }
            
            if (to_physical_layer(data->socket_fd, &s, &data->server_addr) < 0) {
                perror("sendto failed");
                retries++;
                continue;
            }
            
            event_type event = wait_for_event(data->socket_fd, TIMEOUT_US);
            if (event == FRAME_ARRIVAL) {
                if (from_physical_layer(data->socket_fd, &r, &data->server_addr) > 0) {
                    if (r.client_id == data->client_id && r.ack == next_frame_to_send) {
                        acked = 1;
                        gettimeofday(&end, NULL);
                        long long rtt = (end.tv_sec - start.tv_sec) * 1000000LL + 
                                       (end.tv_usec - start.tv_usec);
                        data->total_rtt += rtt;
                        data->rx_cnt++;
                        next_frame_to_send = 1 - next_frame_to_send; // Toggle 0/1
                    }
                }
            } else {
                retries++;
            }
        }
        
        if (!acked) {
            fprintf(stderr, "Client %u: Max retries reached for seq %u\n",
                    data->client_id, next_frame_to_send);
            break;
        }
    }
    
    // Calculate request rate
    if (data->total_rtt > 0) {
        data->request_rate = (float)data->rx_cnt / (data->total_rtt / 1000000.0);
    }
    
    return NULL;
}

void *receiver_thread(void *arg) {
    int sock_fd = *(int *)arg;
    frame r, s;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    uint32_t *expected_seq = calloc(MAX_CLIENTS, sizeof(uint32_t));
    long total_packets = 0;
    struct timeval start, current;
    
    if (!expected_seq) {
        perror("Failed to allocate expected_seq array");
        pthread_exit(NULL);
    }
    
    gettimeofday(&start, NULL);
    
    printf("[SERVER] Ready to receive packets (supports up to %d clients)...\n", MAX_CLIENTS);
    
    while (1) {
        int n = from_physical_layer(sock_fd, &r, &client_addr);
        if (n <= 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recvfrom error");
            }
            continue;
        }
        
        total_packets++;
        uint32_t cid = r.client_id;
        
        // Strict bounds checking
        if (cid == 0 || cid >= MAX_CLIENTS) {
            fprintf(stderr, "Invalid client ID: %u (max allowed: %d)\n", 
                   cid, MAX_CLIENTS-1);
            continue;
        }
        
        // Periodic logging
        if (total_packets % 1000 == 0) {
            gettimeofday(&current, NULL);
            double elapsed = (current.tv_sec - start.tv_sec) + 
                           (current.tv_usec - start.tv_usec) / 1000000.0;
            printf("[SERVER] Received %ld packets (%.2f pkt/sec) Last from client %u seq %u\n",
                   total_packets, total_packets/elapsed, cid, r.seq);
        }
        
        // Process frame
        if (r.seq == expected_seq[cid]) {
            // Deliver to network layer (simulated)
            expected_seq[cid] = 1 - expected_seq[cid];
        }
        
        // Send ACK
        s.client_id = cid;
        s.ack = r.seq;
        if (to_physical_layer(sock_fd, &s, &client_addr) < 0) {
            perror("sendto ack failed");
        }
    }
    
    free(expected_seq);
    return NULL;
}

void run_client() {
    pthread_t threads[num_client_threads];
    client_thread_data_t *thread_data = malloc(num_client_threads * sizeof(client_thread_data_t));
    struct sockaddr_in server_addr;
    
    if (!thread_data) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);
    
    for (int i = 0; i < num_client_threads; i++) {
        // Create UDP socket
        if ((thread_data[i].socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }
        
        // Copy server address
        memcpy(&thread_data[i].server_addr, &server_addr, sizeof(server_addr));
        
        // Assign client ID (1 to MAX_CLIENTS-1)
        thread_data[i].client_id = (i % (MAX_CLIENTS-1)) + 1;
        thread_data[i].total_rtt = 0;
        thread_data[i].tx_cnt = 0;
        thread_data[i].rx_cnt = 0;
        thread_data[i].retransmit_cnt = 0;
        thread_data[i].request_rate = 0.0;
        
        // Create client thread
        if (pthread_create(&threads[i], NULL, sender_thread, &thread_data[i]) != 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }
    }
    
    // Collect statistics
    long long total_rtt = 0;
    long total_tx = 0;
    long total_rx = 0;
    long total_retrans = 0;
    float total_request_rate = 0.0;
    
    for (int i = 0; i < num_client_threads; i++) {
        pthread_join(threads[i], NULL);
        total_rtt += thread_data[i].total_rtt;
        total_tx += thread_data[i].tx_cnt;
        total_rx += thread_data[i].rx_cnt;
        total_retrans += thread_data[i].retransmit_cnt;
        total_request_rate += thread_data[i].request_rate;
        
        printf("Client %u: Sent %ld, Received %ld, Retrans %ld, Loss %.2f%%, Rate %.2f/s\n",
               thread_data[i].client_id,
               thread_data[i].tx_cnt,
               thread_data[i].rx_cnt,
               thread_data[i].retransmit_cnt,
               (thread_data[i].tx_cnt - thread_data[i].rx_cnt) * 100.0 / thread_data[i].tx_cnt,
               thread_data[i].request_rate);
        
        close(thread_data[i].socket_fd);
    }
    
    printf("\nAggregate Statistics:\n");
    printf("Total Packets Sent: %ld\n", total_tx);
    printf("Total Packets Received: %ld\n", total_rx);
    printf("Total Retransmissions: %ld\n", total_retrans);
    printf("Effective Loss Rate: %.2f%%\n", (total_tx - total_rx) * 100.0 / total_tx);
    printf("Average RTT: %lld us\n", total_rtt / (total_rx > 0 ? total_rx : 1));
    printf("Total Request Rate: %.2f messages/s\n", total_request_rate);
    
    free(thread_data);
}

void run_server() {
    int sock_fd;
    struct sockaddr_in server_addr;
    pthread_t receiver_tid;
    
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
    
    printf("[SERVER] Started on %s:%d (Supports %d clients)\n", 
           server_ip, server_port, MAX_CLIENTS);
    
    // Start receiver thread
    if (pthread_create(&receiver_tid, NULL, receiver_thread, &sock_fd) != 0) {
        perror("pthread_create failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    
    pthread_join(receiver_tid, NULL);
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
