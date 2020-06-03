
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
#include <fcntl.h>
  
#define PORT 8080 
#define MAXSEQNUM 25601
#define MAXUDPSIZE 524 
#define HEADERLENGTH 12
#define MAXPAYLOADSIZE 512
#define WINDOWSIZE 10


int sockfd, filefd, n;
struct sockaddr_in servaddr; 
socklen_t len;
char buffer[MAXUDPSIZE];

struct WindowPacket {
    uint16_t acked;
    char buffer[MAXPAYLOADSIZE];
};

struct WindowPacket window[WINDOWSIZE];
uint16_t send_base;
uint16_t send_base_idx;
uint16_t nextseqnum;
uint16_t nextseqnum_idx;

uint16_t AckNum;

struct Header {
    uint16_t SeqNum;
    uint16_t AckNum;
    uint16_t ACK;
    uint16_t SYN;
    uint16_t FIN;
    char zero[2];
} send_ph, rcv_ph;

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
    if (header->ACK && dup)
        printf("DUP-ACK ");
    if (header->ACK)
        printf("ACK ");
    printf("\n");
    
}

int initConnection() {
    memset(&send_ph, 0, sizeof(send_ph));
    srand(time(0));
    uint16_t SeqNum = rand() % MAXSEQNUM;
    send_ph.SeqNum = SeqNum;
    send_ph.SYN = 1;
    printHeader(&send_ph, 1, 0);
    if (sendto(sockfd, (void *)&send_ph, HEADERLENGTH, 
        0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Error sending SYN to server");
        exit(EXIT_FAILURE);
    }
    len = sizeof(servaddr);
    n = recvfrom(sockfd, (char *)buffer, MAXUDPSIZE,  
                MSG_WAITALL, (struct sockaddr *) &servaddr, 
                &len);
    if (n < 0) {
        perror("Error receiving SYN ACK from server");
        exit(EXIT_FAILURE);
    }

    memset(&rcv_ph, 0, sizeof(rcv_ph));
    memcpy(&rcv_ph, &buffer, 12);
    printHeader(&rcv_ph, 0, 0);
    AckNum = rcv_ph.SeqNum + 1;
    send_base = rcv_ph.AckNum;
    nextseqnum = send_base;
    send_base_idx = 0;
    nextseqnum_idx = 0;
    memset(&window, 0, sizeof(window));

    return 0;
}

int sendPacket() {
    memset(&send_ph, 0, sizeof(send_ph));
    send_ph.SeqNum = nextseqnum;
    send_ph.AckNum = AckNum;
    memset(&buffer, 0, sizeof(buffer));
    memcpy(&buffer, &send_ph, HEADERLENGTH);
    n = read(filefd, &buffer[HEADERLENGTH], MAXPAYLOADSIZE);
    if (n < 0) {
        perror("Error reading from filefd");
        exit(EXIT_FAILURE);
    }
    memcpy(&window[nextseqnum_idx].buffer, &buffer[HEADERLENGTH], n);
    if (sendto(sockfd, (void *)&buffer, HEADERLENGTH + n,
            0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Error sending packet to server");
        exit(EXIT_FAILURE);
    }
    printHeader(&send_ph, 1, 0);
    
    
    nextseqnum_idx = (nextseqnum_idx + 1) % WINDOWSIZE;
    nextseqnum = (nextseqnum + n) % MAXSEQNUM;


    if (n < MAXPAYLOADSIZE) // sender has reached end of file
        return 1;
    else
        return 0;
}

int receivePacket() {
    memset(&buffer, 0, sizeof(buffer));
    n = recvfrom(sockfd, buffer, MAXUDPSIZE,  
                MSG_WAITALL, (struct sockaddr *) &servaddr, 
                &len);
    memset(&rcv_ph, 0, sizeof(rcv_ph));
    memcpy(&rcv_ph, &buffer, HEADERLENGTH);
    if (rcv_ph.SeqNum == AckNum)
        AckNum += 1;
    int packet_idx = (modulo(rcv_ph.AckNum - 1 - send_base, MAXSEQNUM) / MAXPAYLOADSIZE + send_base_idx) % WINDOWSIZE; // returns the window index of the ACKed packet
    if (window[packet_idx].acked)
        printHeader(&rcv_ph, 0, 1);
    else 
        printHeader(&rcv_ph, 0, 0);
    
    window[packet_idx].acked = 1;
    if (packet_idx == send_base_idx) {
        int i;
        for (i = 0; i < WINDOWSIZE; i++) {
            if (window[(packet_idx + i) % WINDOWSIZE].acked) {
                window[(packet_idx + i) % WINDOWSIZE].acked = 0;
                send_base = rcv_ph.AckNum;
                send_base_idx = (send_base_idx + 1) % WINDOWSIZE;
                if (sendPacket())
                    return 1;
            }
            else
                break;
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

    initConnection();
    filefd = open(argv[3], O_RDONLY);
    if (sendPacket())
        fprintf(stderr, "Reached end of file\n");
    else {
        while(1) {
            if (receivePacket())
                break;
        }
    }

    
    close(sockfd); 
    close(filefd);
    return 0; 
} 
