
// Server side implementation of UDP client-server model 
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
  
#define PORT     8080 
#define MAXUDPSIZE 524 
#define HEADERLENGTH 12
#define MAXPAYLOADSIZE 512
#define MAXSEQNUM 25601
#define WINDOWSIZE 10
#define WINDOWSLOTS 20

int connection_order = 1;
int connection_established = 0;
int sockfd, filefd, n; 
struct sockaddr_in servaddr, cliaddr; 
socklen_t len;
char buffer[MAXUDPSIZE];
uint16_t SeqNum;

struct WindowPacket {
    uint16_t received;
    uint16_t acceptable;
    uint16_t size;
    char buffer[MAXUDPSIZE - HEADERLENGTH];
};

struct WindowPacket window[WINDOWSLOTS];
uint16_t rcv_base;
uint16_t rcv_base_idx;

struct Header {
    uint16_t SeqNum;
    uint16_t AckNum;
    uint8_t ACK;
    uint8_t SYN;
    uint16_t FIN;
    uint16_t idx;
    char zero[2];
} send_ph, rcv_ph, syn_ph, fin_ph;

int modulo(int x,int N){
    return (x % N + N) %N;
}

void printHeader(struct Header* header, int sender, int dup) {
    if (sender)
        printf("SEND ");
    else
        printf("RECV ");
    printf("%d %d ", header->SeqNum, header->AckNum);
    if (header->SYN)
        printf("SYN ");
    else if (header->FIN)
        printf("FIN ");
    if (dup)
        printf("DUP-");
    if (header->ACK)
        printf("ACK ");
    printf("\n");   
}

void printWindow() {
    int i;
    for (i = 0; i < WINDOWSLOTS; i++) {
        printf("%d:%d%d,", i, window[i].received, window[i].acceptable);
    }
    printf("\n");
}

int initConnection() {
    int first = 1;
    len = sizeof(cliaddr);
    while(1) {
        memset(&rcv_ph, 0, sizeof(rcv_ph));
        n = recvfrom(sockfd, (char *)buffer, MAXUDPSIZE,  
                    MSG_WAITALL, (struct sockaddr *) &cliaddr, 
                    &len); 

        memcpy(&rcv_ph, &buffer, 12);
        printHeader(&rcv_ph, 0, 0);
        if (!rcv_ph.SYN && rcv_ph.FIN == 0) {
            break;
        }
        if (first) {
            uint16_t AckNum = (rcv_ph.SeqNum + 1) % MAXSEQNUM;
            memset(&send_ph, 0, sizeof(send_ph));
            SeqNum = rand() % MAXSEQNUM;
            send_ph.SeqNum = SeqNum;
            send_ph.AckNum = AckNum;
            send_ph.ACK = 1;
            send_ph.SYN = 1;
            memcpy(&syn_ph, &send_ph, sizeof(syn_ph));
            SeqNum += 1;  
            rcv_base = AckNum;
            rcv_base_idx = 0;
            memset(&window, 0, sizeof(window));
            int i;
            for (i = 0; i < WINDOWSIZE; i++) {
                window[i].acceptable = 1;
            }
            char file_name[8];
            sprintf(file_name, "%d.file", connection_order);
            filefd = open(file_name, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
            if (filefd < 0)
                perror("Error opening file to write to");
        }
        
        if (sendto(sockfd, (void *)&syn_ph, sizeof(syn_ph), 
            0, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
            perror("Error sending SYN ACK to client");
            exit(EXIT_FAILURE);
        }
        printHeader(&syn_ph, 1, first ? 0 : 1);
        first = 0;
    }
    return 0;
}

int closeConnection() { // rcv_ph is a FIN packet

    while (1) {
        memset(&send_ph, 0, sizeof(send_ph));
        send_ph.SeqNum = SeqNum;
        send_ph.AckNum = rcv_base;
        send_ph.ACK = 1;
        if (sendto(sockfd, (void *)&send_ph, HEADERLENGTH,
                0, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
            perror("Error sending ACK packet to client");
            exit(EXIT_FAILURE);
        }
        printHeader(&send_ph, 1, 0);
        memset(&send_ph, 0, sizeof(send_ph));
        send_ph.SeqNum = SeqNum;
        send_ph.FIN = 1;
        if (sendto(sockfd, (void *)&send_ph, HEADERLENGTH,
                0, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
            perror("Error sending ACK packet to client");
            exit(EXIT_FAILURE);
        }
        printHeader(&send_ph, 1, 0);
        struct timeval tv;
        gettimeofday(&tv, NULL);
        double now = (double) tv.tv_sec + (double) tv.tv_usec/1000000;
        double timeout = now + 0.5;

        int done = 0;
        while (1) {
            n = recvfrom(sockfd, (char *)buffer, MAXUDPSIZE,  
                        MSG_WAITALL, (struct sockaddr *) &cliaddr, 
                        &len);
            memset(&rcv_ph, 0, sizeof(rcv_ph));
            memcpy(&rcv_ph, &buffer, sizeof(rcv_ph));
            printHeader(&rcv_ph, 0, 0);
            if (rcv_ph.ACK) {
                done = 1;
                break;
            }
            gettimeofday(&tv, NULL);
            now = (double) tv.tv_sec + (double) tv.tv_usec/1000000;
            if (now >= timeout) {
                done = 1;
                break;
            }
        }
        if (done)
            break;
    }
    connection_order += 1;
    connection_established = 0;

    return 0;
}

int sendPacket(uint16_t AckNum, uint16_t idx) {
    memset(&send_ph, 0, sizeof(send_ph));
    send_ph.SeqNum = SeqNum;
    send_ph.AckNum = AckNum;
    send_ph.ACK = 1;
    send_ph.idx = idx;

    if (sendto(sockfd, (void *)&send_ph, HEADERLENGTH,
            0, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
        perror("Error sending ACK packet to client");
        exit(EXIT_FAILURE);
    }
    if (!window[idx].acceptable)
        printHeader(&send_ph, 1, 1);
    else if (window[idx].received)
        printHeader(&send_ph, 1, 1);
    else
        printHeader(&send_ph, 1, 0);
}

int receivePacket() {
    if (connection_established) {
        memset(&buffer, 0, sizeof(buffer));
        n = recvfrom(sockfd, buffer, MAXUDPSIZE, MSG_WAITALL,
                    (struct sockaddr *) &cliaddr, &len);
        memset(&rcv_ph, 0, sizeof(rcv_ph));
        memcpy(&rcv_ph, &buffer, HEADERLENGTH);
        printHeader(&rcv_ph, 0, 0);
    }
    connection_established = 1;
    if (rcv_ph.FIN) {
        rcv_base += 1;
        return 1;
    }
    int payload_len = n - HEADERLENGTH;

    if (!window[rcv_ph.idx].acceptable) {
        sendPacket((rcv_ph.SeqNum + payload_len) % MAXSEQNUM, rcv_ph.idx);
        return 0;
    }

    int packet_idx = rcv_ph.idx;
    memcpy(&window[packet_idx].buffer, &buffer[HEADERLENGTH], payload_len);
    window[packet_idx].size = payload_len;
    sendPacket((rcv_ph.SeqNum + payload_len) % MAXSEQNUM, packet_idx);
    window[packet_idx].received = 1;
    if (rcv_ph.SeqNum == rcv_base) {
        int i;
        for (i = 0; i < WINDOWSIZE; i++) {
            int curr_idx = (packet_idx + i) % WINDOWSLOTS;
            if (window[curr_idx].received) {
                window[curr_idx].received = 0;
                window[curr_idx].acceptable = 0;
                write(filefd, &window[curr_idx].buffer, window[curr_idx].size);
                rcv_base = (rcv_base + window[curr_idx].size) % MAXSEQNUM;
                window[curr_idx].size = 0;
                window[(rcv_base_idx + WINDOWSIZE) % WINDOWSLOTS].acceptable = 1;
                rcv_base_idx = (rcv_base_idx + 1) % WINDOWSLOTS;
            }
            else
                break;
        }
    }
    return 0;
}
  
  
int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./server <port number>\n");
        exit(1);
    }
    srand(time(0));
      
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
      
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    // Filling server information 
    servaddr.sin_family = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons((uint16_t)atoi(argv[1]));
      
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 ) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    while(1) {
        initConnection();
        while (1) {
            if (receivePacket())
                break;
        }
        closeConnection();
        close(filefd);
        printf("----\n");
    }

    close(sockfd);
    return 0; 
} 
