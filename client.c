
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
  
#define PORT     8080 
#define MAXLINE 1024 
#define MAXSEQNUM 25600

int sockfd;
struct sockaddr_in servaddr; 


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
    srand(time(0));
    packet_header.SeqNum = (rand() % MAXSEQNUM) % MAXSEQNUM;
    packet_header.SYN = 1;
    printf("SYN packet to server: ");
    printHeader(&packet_header);
    if (sendto(sockfd, (void *)&packet_header, sizeof(packet_header), 
        0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Error sending SYN to server");
        exit(EXIT_FAILURE);
    }
    int n;
    socklen_t len = sizeof(servaddr);
    n = recvfrom(sockfd, (char *)buffer, MAXLINE,  
                MSG_WAITALL, (struct sockaddr *) &servaddr, 
                &len);
    if (n < 0) {
        perror("Error receiving SYNACK from server");
        exit(EXIT_FAILURE);
    }

    memset(&packet_header, 0, sizeof(packet_header));
    memcpy(&packet_header, &buffer, 12);
    printf("SYNACK from server: ");
    printHeader(&packet_header);
    return 0;
}
  
int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./client <server IP address> <port number> <object requested>\n");
        exit(1);
    }
    srand(time(0));
  
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
    
    /*
    int n, len, sent; 
      
    sent = sendto(sockfd, (const char *)hello, strlen(hello), 
        0, (const struct sockaddr *) &servaddr,  
            sizeof(servaddr)); 
    if (sent < 0) {
        perror("Error on sendto");
        exit(EXIT_FAILURE);
    }

    printf("Hello message sent.\n"); 
          
    n = recvfrom(sockfd, (char *)buffer, MAXLINE,  
                MSG_WAITALL, (struct sockaddr *) &servaddr, 
                &len); 
    buffer[n] = '\0'; 
    printf("Server : %s\n", buffer); 
    */
  
    close(sockfd); 
    return 0; 
} 
