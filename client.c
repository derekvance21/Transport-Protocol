
// Client side implementation of UDP client-server model 
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
  
#define PORT 8080 
#define MAXSEQNUM 25601
#define MAXUDPSIZE 524 
#define HEADERLENGTH 12
#define MAXPAYLOADSIZE 512
#define WINDOWSIZE 10
#define TIMEOUT 0.5
#define SHUTDOWNTIME 2.0
#define WINDOWSLOTS 20


int sockfd, filefd, n;
struct sockaddr_in servaddr; 
socklen_t len;
char buffer[MAXUDPSIZE];

struct WindowPacket {
    uint16_t acked;
    uint16_t sent;
    uint16_t size;
    uint16_t SeqNum;
    char header[HEADERLENGTH];
    char buffer[MAXPAYLOADSIZE];
};

struct WindowPacket window[WINDOWSLOTS];
struct timeval tv;
double timeout_window[WINDOWSLOTS];
double timeout_cntrl;
uint16_t send_base;
uint16_t send_base_idx;
uint16_t nextseqnum;
uint16_t nextseqnum_idx;

uint16_t AckNum;
uint16_t isACK;

struct Header {
    uint16_t SeqNum;
    uint16_t AckNum;
    uint8_t ACK;
    uint8_t SYN;
    uint16_t FIN;
    uint16_t idx;
    char zero[2];
} send_ph, rcv_ph;

int modulo(int x,int N){
    return (x % N + N) %N;
}

double gettime() {
    gettimeofday(&tv, NULL);
    return (double) tv.tv_sec + (double) tv.tv_usec/1000000;
}

int isTimeout(int i) {
    double now = gettime();
    double timeout;
    if (i < 0)
        timeout = timeout_cntrl;
    else
        timeout = timeout_window[i];

    if (now < timeout)
        return 0;
    else 
        return 1;
}

void printHeader(struct Header* header, int sender, int resend) {
    if (sender && resend)
        printf("RE");
    if (sender)
        printf("SEND ");
    else
        printf("RECV ");
    printf("%d %d ", header->SeqNum, header->AckNum);
    if (header->SYN)
        printf("SYN ");
    else if (header->FIN)
        printf("FIN ");
    // if (header->ACK && dup) should be only for server i think
    //     printf("DUP-ACK ");
    if (header->ACK)
        printf("ACK ");
    printf("(%d)", header->idx);
    printf("\n");
}

void printWindow() {
    int i;
    for (i = 0; i < WINDOWSLOTS; i++) {
        // idx = (i + rcv_base_idx) % WINDOWSLOTS;
        printf("s:%d,a:%d", window[i].sent, window[i].acked);
    }
}

int initConnection() {
    memset(&send_ph, 0, sizeof(send_ph));
    srand(time(0));
    uint16_t SeqNum = rand() % MAXSEQNUM;
    send_ph.SeqNum = SeqNum;
    send_ph.SYN = 1;
    int resend = 0;
    len = sizeof(servaddr);

    while (1) {
        if (sendto(sockfd, (void *)&send_ph, HEADERLENGTH, 
            0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            perror("Error sending SYN to server");
            exit(EXIT_FAILURE);
        }
        timeout_cntrl = gettime() + TIMEOUT;
        printHeader(&send_ph, 1, resend);
        resend = 1;
        while (1) {
            n = recvfrom(sockfd, (char *)buffer, MAXUDPSIZE,  
                        MSG_WAITALL, (struct sockaddr *) &servaddr, 
                        &len);
            // if (n < 0) { // nothing to read from socket
            //     perror("Error receiving SYN ACK from server");
            // }
            if (n > 0) {
                memset(&rcv_ph, 0, sizeof(rcv_ph));
                memcpy(&rcv_ph, &buffer, 12);
                break;
            }
            else if (isTimeout(-1)) {
                printf("TIMEOUT %d\n", SeqNum);
                break;
            }
        }
        if (n > 0) break;
    }
    printHeader(&rcv_ph, 0, 0);
    AckNum = rcv_ph.SeqNum + 1;
    send_base = rcv_ph.AckNum;
    nextseqnum = send_base;
    send_base_idx = 0;
    nextseqnum_idx = 0;
    isACK = 1;
    memset(&window, 0, sizeof(window));

    return 0;
}

int sendFIN(int resend) {
    memset(&send_ph, 0, sizeof(send_ph));
    send_ph.SeqNum = nextseqnum;
    send_ph.FIN = 1;
    if (sendto(sockfd, (void *)&send_ph, HEADERLENGTH, 
        0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Error sending SYN to server");
        exit(EXIT_FAILURE);
    }
    printHeader(&send_ph, 1, resend);
    timeout_cntrl = gettime() + TIMEOUT;
}

int closeConnection(int resend) {
    while (1) {
        sendFIN(resend);
        resend = 1;
        int cont = 0;
        while (1) { // expect the ACK for the FIN
            n = recvfrom(sockfd, (char *)buffer, MAXUDPSIZE,  
                        MSG_WAITALL, (struct sockaddr *) &servaddr, 
                        &len);
            // if (n < 0) {
            //     perror("Error receiving SYN ACK from server");
            //     exit(EXIT_FAILURE);
            // }
            if (n > 0) {
                memset(&rcv_ph, 0, sizeof(rcv_ph));
                memcpy(&rcv_ph, &buffer, HEADERLENGTH);
                printHeader(&rcv_ph, 0, 0);
                if (rcv_ph.ACK && (nextseqnum + 1) % MAXSEQNUM == rcv_ph.AckNum) {
                    cont = 1;
                    break;
                }
            }
            else if (isTimeout(-1)) {
                printf("TIMEOUT %d\n", nextseqnum);
                break;
            }
        }
        if (n > 0 && cont) break;
    }
    while (1) { // expect the FIN
        n = recvfrom(sockfd, (char *)buffer, MAXUDPSIZE,
                MSG_WAITALL, (struct sockaddr *) &servaddr,
                &len);
        if (n > 0) {
            memset(&rcv_ph, 0, sizeof(rcv_ph));
            memcpy(&rcv_ph, &buffer, 12);
            printHeader(&rcv_ph, 0, 0);
            if (!rcv_ph.FIN)
                continue;
            else
                break;
        }
        else if (isTimeout(-1)) {
            printf("TIMEOUT %d\n", nextseqnum);
            closeConnection(1);
        }
    }
    nextseqnum += 1;

    
    AckNum = rcv_ph.SeqNum + 1;
    // received server FIN
    memset(&send_ph, 0, sizeof(send_ph));
    send_ph.SeqNum = nextseqnum;
    send_ph.AckNum = AckNum;
    send_ph.ACK = 1;
    
    if (sendto(sockfd, (void *)&send_ph, HEADERLENGTH, 
        0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Error sending SYN to server");
        exit(EXIT_FAILURE);
    }
    printHeader(&send_ph, 1, 0);
    timeout_cntrl = gettime() + SHUTDOWNTIME;
    nextseqnum = (nextseqnum + 1) % MAXSEQNUM; // don't think  i need this
    while (1) {
        if (isTimeout(-1))
            break;
    }
}

int sendPacket(int resend, int packet_idx) {
    memset(&send_ph, 0, sizeof(send_ph));
    if (isACK) {
        send_ph.ACK = 1;
        isACK = 0;
    }
    send_ph.AckNum = AckNum;
    memset(&buffer, 0, sizeof(buffer));


    if (resend) {
        send_ph.SeqNum = window[packet_idx].SeqNum;
        send_ph.idx = packet_idx;
        memcpy(&buffer, &send_ph, HEADERLENGTH);

        memcpy(&buffer[HEADERLENGTH], &window[packet_idx].buffer, window[packet_idx].size);
    }
    else {
        send_ph.SeqNum = nextseqnum;
        send_ph.idx = nextseqnum_idx;
        memcpy(&buffer, &send_ph, HEADERLENGTH);
        n = read(filefd, &buffer[HEADERLENGTH], MAXPAYLOADSIZE);
        if (n < 0) {
            perror("Error reading from filefd");
            exit(EXIT_FAILURE);
        }
        if (n == 0)
            return 1;
        memcpy(&window[nextseqnum_idx].buffer, &buffer[HEADERLENGTH], n);
        window[nextseqnum_idx].size = n;
        window[nextseqnum_idx].sent = 1;
        window[nextseqnum_idx].acked = 0;
        window[nextseqnum_idx].SeqNum = nextseqnum;
    }
    uint16_t payload_size = resend ? window[packet_idx].size : window[nextseqnum_idx].size;
    // fprintf(stderr, "payload_size: %d, ", payload_size);
    if (sendto(sockfd, (void *)&buffer, HEADERLENGTH + payload_size,
            0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            perror("Error sending packet to server");
            exit(EXIT_FAILURE);
    }
    if (resend) {
        timeout_window[packet_idx] = gettime() + TIMEOUT;
    }
    if (!resend) {
        timeout_window[nextseqnum_idx] = gettime() + TIMEOUT;
        nextseqnum_idx = (nextseqnum_idx + 1) % WINDOWSLOTS;
        nextseqnum = (nextseqnum + n) % MAXSEQNUM;
    }

    printHeader(&send_ph, 1, resend);

    if (n < MAXPAYLOADSIZE) { // sender has reached end of file
        return 1;
    }
    else
        return 0;
}

int receivePacket() {
    int resend = 0;
    while(1) {
        memset(&buffer, 0, sizeof(buffer));
        n = recvfrom(sockfd, buffer, MAXUDPSIZE,  
                    MSG_WAITALL, (struct sockaddr *) &servaddr, 
                    &len);
        if (n > 0) {
            memset(&rcv_ph, 0, sizeof(rcv_ph));
            memcpy(&rcv_ph, &buffer, HEADERLENGTH);
            // if (rcv_ph.SeqNum == AckNum) {
            //     AckNum += 1;
            // }
            if (!window[rcv_ph.idx].sent) {
                printHeader(&rcv_ph, 0, 1);
                continue;
            }
            // int packet_idx = (modulo(rcv_ph.AckNum - 1 - send_base, MAXSEQNUM) / MAXPAYLOADSIZE + send_base_idx) % WINDOWSIZE; // returns the window index of the ACKed packet
            int packet_idx = rcv_ph.idx;
            if (window[packet_idx].acked)
                printHeader(&rcv_ph, 0, 0);
            else 
                printHeader(&rcv_ph, 0, 0);
            // fprintf(stderr, "packet_idx: %d, ", packet_idx);
            window[packet_idx].acked = 1;
            int orig_nextseqnum_idx = nextseqnum_idx;
            if (packet_idx == send_base_idx) {
                int i;
                for (i = 0; i < WINDOWSIZE; i++) {
                    if (window[(packet_idx + i) % WINDOWSLOTS].acked) {
                        //fprintf(stderr, "packet is acked\n");
                        window[(packet_idx + i) % WINDOWSLOTS].acked = 0;
                        window[(packet_idx + i) % WINDOWSLOTS].sent = 0;
                        send_base = rcv_ph.AckNum;
                        send_base_idx = (send_base_idx + 1) % WINDOWSLOTS;
                        sendPacket(0, nextseqnum_idx);
                    }
                    else
                        break;
                }
                // fprintf(stderr, "i: %d, sb_idx %d, nsn_idx: %d\n", i, send_base_idx, nextseqnum_idx);
                if (i == modulo(orig_nextseqnum_idx - packet_idx, WINDOWSLOTS)) {
                    return 1;
                }
            }
        }
        else {
            int i;
            for (i = 0; i < WINDOWSIZE; i++) {
                int packet_idx = (i + send_base_idx) % WINDOWSLOTS;
                if (!window[packet_idx].acked && window[packet_idx].sent && isTimeout(packet_idx)) {
                    printf("TIMEOUT %d (%d)\n", (i * MAXPAYLOADSIZE + send_base) % MAXSEQNUM, packet_idx);
                    sendPacket(1, packet_idx);
                }
            }
        }
    }
    return 0;
}
  
int main(int argc, char** argv) {
    
    if (argc != 4) {
        fprintf(stderr, "Usage: ./client <server IP address> <port number> <object requested>\n");
        exit(1);
    }

    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons((uint16_t)atoi(argv[2]));
    servaddr.sin_addr.s_addr = inet_addr(argv[1]); 

    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    initConnection();
    filefd = open(argv[3], O_RDONLY);

    int i;
    for (i = 0; i < WINDOWSIZE; i++) {
        if (sendPacket(0, -1)) {
            break;
        }
    }
    while(1) {
        if (receivePacket())
            break;
    }
    closeConnection(0);

    
    close(sockfd); 
    close(filefd);
    return 0; 
} 