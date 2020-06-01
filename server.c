
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
#define MAXLINE 1024 

#define MAXSEQNUM 25601

int sockfd; 
struct sockaddr_in servaddr, cliaddr; 


struct Header {
    uint16_t SeqNum;
    uint16_t AckNum;
    uint16_t ACK;
    uint16_t SYN;
    uint16_t FIN;
    char zero[2];
};

void printHeader(struct Header* header) {
    printf("%d %d %d %d %d\n", header->SeqNum, header->AckNum, header->ACK, header->SYN, header->FIN);
}

int initConnection() {
    char buffer[MAXLINE];
    struct Header packet_header;
    memset(&packet_header, 0, sizeof(packet_header));
    
    int n;
    socklen_t len = sizeof(cliaddr);

    n = recvfrom(sockfd, (char *)buffer, MAXLINE,  
                MSG_WAITALL, (struct sockaddr *) &cliaddr, 
                &len); 

    memcpy(&packet_header, &buffer, 12);
    printf("SYN packet from client: ");
    printHeader(&packet_header);
    if (packet_header.SYN != 1 || packet_header.FIN != 0 || packet_header.SeqNum >= MAXSEQNUM) {
        fprintf(stderr, "Connection establishment failed due to invalid SYN packet\n");
        return -1;
    }
    uint16_t cliSeqNum = packet_header.SeqNum;
    memset(&packet_header, 0, sizeof(packet_header));
    packet_header.SeqNum = rand() % MAXSEQNUM;
    packet_header.AckNum = (cliSeqNum + 1) % MAXSEQNUM;
    packet_header.ACK = 1;
    packet_header.SYN = 1;
    printf("SYNACK packet to client: ");
    printHeader(&packet_header);
    if (sendto(sockfd, (void *)&packet_header, sizeof(packet_header), 
        0, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
        perror("Error sending SYNACK to client");
        exit(EXIT_FAILURE);
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
    /*  
    int len, n; 
  
    len = sizeof(cliaddr);  //len is value/resuslt 
  
    n = recvfrom(sockfd, (char *)buffer, MAXLINE,  
                MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
                &len); 
    buffer[n] = '\0'; 
    printf("Client : %s\n", buffer); 
    sendto(sockfd, (const char *)hello, strlen(hello),  
        0, (const struct sockaddr *) &cliaddr, 
            len); 
    printf("Hello message sent.\n");  
    */
    return 0; 
} 
