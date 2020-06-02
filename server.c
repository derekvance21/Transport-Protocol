
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
#define MAXSEQNUM 25601

int connection_order = 1;
int sockfd, filefd; 
struct sockaddr_in servaddr, cliaddr; 
char buffer[MAXUDPSIZE];
uint16_t AckNum, SeqNum;


struct WindowPacket {
    uint16_t received;
    char buffer[MAXUDPSIZE - HEADERLENGTH];
};

struct WindowPacket window[10];
uint16_t rcv_base;

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
    int n;
    socklen_t len = sizeof(cliaddr);

    n = recvfrom(sockfd, (char *)buffer, MAXUDPSIZE,  
                MSG_WAITALL, (struct sockaddr *) &cliaddr, 
                &len); 

    memcpy(&packet_header, &buffer, 12);
    printHeader(&packet_header, 0, 0);
    AckNum = packet_header.SeqNum;
    memset(&packet_header, 0, sizeof(packet_header));
    SeqNum = rand() % MAXSEQNUM;
    packet_header.SeqNum = SeqNum;
    packet_header.AckNum = (AckNum + 1) % MAXSEQNUM;
    packet_header.ACK = 1;
    packet_header.SYN = 1;
    
    if (sendto(sockfd, (void *)&packet_header, sizeof(packet_header), 
        0, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
        perror("Error sending SYNACK to client");
        exit(EXIT_FAILURE);
    }
    printHeader(&packet_header, 1, 0);

    return 0;
}

int sendPacket() {
    memset(&packet_header, 0, sizeof(packet_header));
    
}

int receivePacket() {

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
    servaddr.sin_family    = AF_INET; // IPv4 
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

    socklen_t len = sizeof(cliaddr);
    int n = recvfrom(sockfd, (char *)buffer, MAXUDPSIZE,  
                MSG_WAITALL, (struct sockaddr *) &cliaddr, 
                &len); 
    memcpy(&packet_header, &buffer, 12);
    printHeader(&packet_header, 0, 0);
    char file_name[8];
    sprintf(file_name, "%d.file", connection_order);
    if (n > 12) {
        filefd = open(file_name, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
        if (filefd < 0)
            perror("Error opening file to write to");
        write(filefd, &buffer[12], n - 12);
    }
    
    return 0; 
} 
