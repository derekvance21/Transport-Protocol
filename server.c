
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
#include <fcntl.h>
  
#define PORT     8080 
#define MAXUDPSIZE 524 
#define HEADERLENGTH 12
#define MAXPAYLOADSIZE 512
#define MAXSEQNUM 25601
#define WINDOWSIZE 10

int connection_order = 1;
int sockfd, filefd, n; 
struct sockaddr_in servaddr, cliaddr; 
socklen_t len;
char buffer[MAXUDPSIZE];
uint16_t SeqNum;


struct WindowPacket {
    uint16_t received;
    char buffer[MAXUDPSIZE - HEADERLENGTH];
};

struct WindowPacket window[10];
uint16_t rcv_base;
uint16_t rcv_base_idx;

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
    memset(&rcv_ph, 0, sizeof(rcv_ph));
    len = sizeof(cliaddr);

    n = recvfrom(sockfd, (char *)buffer, MAXUDPSIZE,  
                MSG_WAITALL, (struct sockaddr *) &cliaddr, 
                &len); 

    memcpy(&rcv_ph, &buffer, 12);
    printHeader(&rcv_ph, 0, 0);
    uint16_t AckNum = (rcv_ph.SeqNum + 1) % MAXSEQNUM;
    memset(&send_ph, 0, sizeof(send_ph));
    SeqNum = rand() % MAXSEQNUM;
    send_ph.SeqNum = SeqNum;
    send_ph.AckNum = AckNum;
    send_ph.ACK = 1;
    send_ph.SYN = 1;
    
    if (sendto(sockfd, (void *)&send_ph, sizeof(send_ph), 
        0, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
        perror("Error sending SYN ACK to client");
        exit(EXIT_FAILURE);
    }
    printHeader(&send_ph, 1, 0);
    SeqNum += 1;
    rcv_base = AckNum;
    rcv_base_idx = 0;

    memset(&window, 0, sizeof(window));

    char file_name[8];
    sprintf(file_name, "%d.file", connection_order);
    filefd = open(file_name, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if (filefd < 0)
        perror("Error opening file to write to");
    connection_order += 1;

    return 0;
}

int sendPacket(uint16_t AckNum) {
    memset(&send_ph, 0, sizeof(send_ph));
    send_ph.SeqNum = SeqNum;
    send_ph.AckNum = AckNum;
    send_ph.ACK = 1;
    if (sendto(sockfd, (void *)&send_ph, HEADERLENGTH,
            0, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
        perror("Error sending ACK packet to client");
        exit(EXIT_FAILURE);
    }
    SeqNum += 1;
    int packet_idx = (modulo(AckNum - 1 - rcv_base, MAXSEQNUM) / MAXPAYLOADSIZE + rcv_base_idx) % WINDOWSIZE;
    if (window[packet_idx].received)
        printHeader(&send_ph, 1, 1);
    else
        printHeader(&send_ph, 1, 0);
}

int receivePacket() {
    memset(&buffer, 0, sizeof(buffer));
    n = recvfrom(sockfd, buffer, MAXUDPSIZE, MSG_WAITALL,
                (struct sockaddr *) &cliaddr, &len);
    memset(&rcv_ph, 0, sizeof(rcv_ph));
    memcpy(&rcv_ph, &buffer, HEADERLENGTH);
    int packet_idx = ((rcv_ph.SeqNum - rcv_base) / MAXPAYLOADSIZE + rcv_base_idx) % WINDOWSIZE;
    int payload_len = n - HEADERLENGTH;
    memcpy(&window[packet_idx].buffer, &buffer[HEADERLENGTH], payload_len);
    printHeader(&rcv_ph, 0, 0);
    sendPacket((rcv_ph.SeqNum + payload_len) % MAXSEQNUM);
    window[packet_idx].received = 1;
    if (rcv_ph.SeqNum == rcv_base) {
        int i;
        for (i = 0; i < WINDOWSIZE; i++) {
            int curr_idx = (packet_idx + i) % WINDOWSIZE;
            if (window[curr_idx].received) {
                window[curr_idx].received = 0;
                write(filefd, &window[curr_idx].buffer, payload_len);
                rcv_base = (rcv_base + payload_len) % MAXSEQNUM;
                rcv_base_idx = (rcv_base_idx + 1) % WINDOWSIZE;
            }
            else
                break;
        }
    }
    if (n < MAXPAYLOADSIZE)
        return 1;
    else
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
    // fcntl(sockfd, F_SETFL, O_NONBLOCK);
      
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

    initConnection();
    while (1) {
        if (receivePacket())
            break;
    }
    
    close(sockfd);
    close(filefd);
    return 0; 
} 
