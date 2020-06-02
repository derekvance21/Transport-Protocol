
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
#define MAXPAYLOADSIZE MAXUDPSIZE - HEADERLENGTH
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
} packet_header;

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
    memset(&packet_header, 0, sizeof(packet_header));
    srand(time(0));
    uint16_t SeqNum = rand() % MAXSEQNUM;
    packet_header.SeqNum = SeqNum;
    packet_header.SYN = 1;
    printHeader(&packet_header, 1, 0);
    if (sendto(sockfd, (void *)&packet_header, sizeof(packet_header), 
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

    memset(&packet_header, 0, sizeof(packet_header));
    memcpy(&packet_header, &buffer, 12);
    printHeader(&packet_header, 0, 0);
    AckNum = packet_header.SeqNum + 1;
    send_base = packet_header.AckNum;
    nextseqnum = send_base;
    send_base_idx = 0;
    nextseqnum_idx = 0;
    memset(&window, 0, sizeof(window));

    return 0;
}

int sendPacket() {
    memset(&packet_header, 0, sizeof(packet_header));
    packet_header.SeqNum = nextseqnum;
    packet_header.AckNum = AckNum;
    memset(&buffer, 0, sizeof(buffer));
    memcpy(&buffer, &packet_header, HEADERLENGTH);
    n = read(filefd, &buffer[HEADERLENGTH], MAXPAYLOADSIZE);
    if (n < 0) {
        perror("Error reading from filefd");
        exit(EXIT_FAILURE);
    }
    memcpy(&window[nextseqnum_idx], &buffer[HEADERLENGTH], n);
    if (sendto(sockfd, (void *)&buffer, HEADERLENGTH + n,
            0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Error sending packet to server");
        exit(EXIT_FAILURE);
    }
    printHeader(&packet_header, 1, 0);
    
    
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
    if (n > HEADERLENGTH)
        fprintf(stderr, "Payload received from server:\n%s\n", &buffer[12]);
    memset(&packet_header, 0, sizeof(packet_header));
    memcpy(&packet_header, &buffer, HEADERLENGTH);
    if (packet_header.SeqNum == AckNum)
        AckNum += 1;
    int packet_idx = (packet_header.AckNum - 1 - send_base) / MAXPAYLOADSIZE; // returns the window index of the ACKed packet
    if (window[packet_idx].acked)
        printHeader(&packet_header, 0, 1);
    else 
        printHeader(&packet_header, 0, 0);
    
    window[packet_idx].acked = 1;
    if (packet_idx == send_base_idx) {
        int i;
        for (i = 0; i < WINDOWSIZE; i++) {
            if (window[(packet_idx + i) % WINDOWSIZE].acked) {
                send_base = packet_header.AckNum;
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
