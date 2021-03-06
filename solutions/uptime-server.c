// CPU Monitor TCP Server - threaded version
// Get server uptime since last reboot
// connect with telnet or nc
// nr@bulme.at 2020
// build with gcc uptime-server.c -o uptime-server -lpthread -Wall

#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

// also try other status messages in /proc - Files
// first float in line is relevant -> uptime in seconds
#define PROC_FILE "/proc/uptime"
#define LINELEN 80
#define BUFSIZE 1000
#define TSAMPLE 1200002 // time between measurements in us

// error - wrapper for perror
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

// read proc file and return hours / minutes / seconds in buffer
void get_uptime(char* buffer)
{
    FILE* fh;
    float dummy;
    int uptime, fraction;
    fh = fopen(PROC_FILE, "r");
    if (fh == NULL)
        error("Cannot open status file\n");
    fscanf(fh, "%d.%d %f", &uptime, &fraction, &dummy);
    fclose(fh);

    // seconds to hours: minutes: seconds
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = (uptime % 3600) % 60;

    sprintf(buffer, "%03d:%02d:%02d.%d\n", hours, minutes, seconds, fraction);
    usleep(TSAMPLE);
}

// called on client connect, send formatted string with CPU status
// Parameter: client socket
void* monitor(void* client)
{
    int clientfd = *(int *)client;
    char outbuffer[BUFSIZE];

    while (1) {
        bzero(outbuffer, sizeof(outbuffer));
        get_uptime(outbuffer);

        // when client has disconnected, dont send broken pipe signal
        if(send(clientfd, outbuffer, sizeof(outbuffer), MSG_NOSIGNAL) == -1)
            break;
    }

    close(clientfd);
    return client;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: %s <portnumber>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int server_socket, rec_socket;
    unsigned int len;
    struct sockaddr_in serverinfo, clientinfo;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    serverinfo.sin_family = AF_INET;
    serverinfo.sin_addr.s_addr = htonl(INADDR_ANY);
    serverinfo.sin_port = htons(port);

    // suppresses "address already in use" message
    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    if (bind(server_socket, (struct sockaddr *)&serverinfo, sizeof(serverinfo)) != 0)
        error("Error bind socket\n");
    listen(server_socket, 3);

    pthread_t child;
    while(1) {
        printf("Server waiting...\n");
        rec_socket = accept(server_socket, (struct sockaddr *)&clientinfo, &len);

        if (pthread_create(&child, NULL, monitor, &rec_socket) != 0)
            error("Error creating thread");
        else
            pthread_detach(child);
    }
    return 0;
}
