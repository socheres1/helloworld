/* Minimal Windows Port Scanner - No Dependencies */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_TIMEOUT 100  /* milliseconds */
#define MAX_THREADS 50       /* Keep low for no admin privileges */

/* Thread data structure */
typedef struct {
    char ip[16];
    int port;
} ScanTask;

/* Simple non-blocking connect with timeout */
int try_connect(const char *ip, int port, int timeout_ms) {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;
    int result = 0;
    
    /* Initialize Winsock */
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        return 0;
    }
    
    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return 0;
    }
    
    /* Setup server address */
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);
    
    /* Set socket to non-blocking */
    unsigned long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
    
    /* Try connect */
    connect(sock, (struct sockaddr*)&server, sizeof(server));
    
    /* Wait for connection with timeout */
    fd_set fdset;
    struct timeval tv;
    
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    tv.tv_sec = 0;
    tv.tv_usec = timeout_ms * 1000; /* Convert to microseconds */
    
    if (select(0, NULL, &fdset, NULL, &tv) == 1) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
        if (so_error == 0) {
            result = 1; /* Success */
        }
    }
    
    /* Cleanup */
    closesocket(sock);
    WSACleanup();
    
    return result;
}

/* Worker thread function */
DWORD WINAPI scan_thread(LPVOID param) {
    ScanTask *task = (ScanTask*)param;
    
    if (try_connect(task->ip, task->port, DEFAULT_TIMEOUT)) {
        printf("%s:%d - OPEN\n", task->ip, task->port);
    }
    
    free(task);
    return 0;
}

int main(int argc, char *argv[]) {
    char ip[16] = "127.0.0.1";
    int start_port = 1;
    int end_port = 1024;
    HANDLE threads[MAX_THREADS];
    int thread_count = 0;
    
    /* Parse command line arguments */
    if (argc >= 2) {
        strncpy(ip, argv[1], sizeof(ip)-1);
        ip[sizeof(ip)-1] = '\0';
    }
    if (argc >= 3) {
        start_port = atoi(argv[2]);
        end_port = start_port;
    }
    if (argc >= 4) {
        end_port = atoi(argv[3]);
    }
    
    printf("Scanning %s ports %d-%d...\n", ip, start_port, end_port);
    
    /* Scan ports */
    for (int port = start_port; port <= end_port; port++) {
        /* Wait if we have too many threads */
        if (thread_count >= MAX_THREADS) {
            WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);
            for (int i = 0; i < thread_count; i++) {
                CloseHandle(threads[i]);
            }
            thread_count = 0;
        }
        
        /* Create thread for this port */
        ScanTask *task = (ScanTask*)malloc(sizeof(ScanTask));
        if (task == NULL) continue;
        
        strncpy(task->ip, ip, sizeof(task->ip));
        task->ip[sizeof(task->ip)-1] = '\0';
        task->port = port;
        
        threads[thread_count] = CreateThread(NULL, 0, scan_thread, task, 0, NULL);
        if (threads[thread_count] != NULL) {
            thread_count++;
        } else {
            free(task);
        }
    }
    
    /* Wait for remaining threads */
    if (thread_count > 0) {
        WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);
        for (int i = 0; i < thread_count; i++) {
            CloseHandle(threads[i]);
        }
    }
    
    printf("Scan complete.\n");
    return 0;
} 
