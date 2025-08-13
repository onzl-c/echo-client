// echo-server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>

#define BUF_SIZE 1024
#define MAX_CLNT 256

// 전역 변수: 클라이언트 소켓 배열 및 개수
int g_client_socks[MAX_CLNT];
int g_client_count = 0;

// 뮤텍스
pthread_mutex_t g_mutex;

// 서버 옵션
bool g_echo_mode = false;
bool g_broadcast_mode = false;

void error_handling(char *message);
void send_msg_to_all(char *msg, int len, int sender_sock);
void *handle_clnt(void *arg);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Syntax : %s <port> [-e [-b]]\n", argv[0]);
        exit(1);
    }

    // 옵션 파싱
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0) {
            g_echo_mode = true;
        } else if (strcmp(argv[i], "-b") == 0) {
            g_broadcast_mode = true;
        }
    }

    if (g_broadcast_mode && !g_echo_mode) {
        printf("Error: -b option requires -e option.\n");
        exit(1);
    }

    // 뮤텍스 초기화
    pthread_mutex_init(&g_mutex, NULL);

    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    // SOCK_STREAM: TCP, SOCK_DGRAM: UDP
    if (serv_sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1) {
        error_handling("bind() error");
    }

    if (listen(serv_sock, 5) == -1) {
        error_handling("listen() error");
    }

    printf("Server Started... (Port: %s)\n", argv[1]);
    if (g_broadcast_mode) printf("Mode: Echo & Broadcast\n");
    else if (g_echo_mode) printf("Mode: Echo\n");


    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        if (clnt_sock == -1) {
            continue; // accept 에러 시 루프 계속
        }

        // --- 임계 영역 시작 ---
        pthread_mutex_lock(&g_mutex);
        if (g_client_count >= MAX_CLNT) {
            printf("Too many clients. Connection rejected.\n");
            close(clnt_sock);
            pthread_mutex_unlock(&g_mutex);
            continue;
        }
        g_client_socks[g_client_count++] = clnt_sock;
        pthread_mutex_unlock(&g_mutex);
        // --- 임계 영역 끝 ---

        // 클라이언트 처리를 위한 스레드 생성
        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);
        pthread_detach(t_id); // 스레드 종료 시 자원 자동 해제

        printf("Connected client IP: %s, Socket: %d\n", inet_ntoa(clnt_adr.sin_addr), clnt_sock);
    }

    close(serv_sock);
    pthread_mutex_destroy(&g_mutex);
    return 0;
}

// 클라이언트 처리 함수 (스레드로 동작)
void *handle_clnt(void *arg) {
    int clnt_sock = *((int *)arg);
    int str_len = 0;
    char msg[BUF_SIZE];

    while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) {
        msg[str_len] = 0;

        // 서버 화면에 메시지 출력 (동기화)
        pthread_mutex_lock(&g_mutex);
        printf("Message from client (sock:%d): %s", clnt_sock, msg);
        pthread_mutex_unlock(&g_mutex);

        if (g_echo_mode) {
             send_msg_to_all(msg, str_len, clnt_sock);
        }
    }

    // 클라이언트 연결 종료 처리
    // --- 임계 영역 시작 ---
    pthread_mutex_lock(&g_mutex);
    for (int i = 0; i < g_client_count; i++) {
        if (clnt_sock == g_client_socks[i]) {
            // 연결 종료된 클라이언트를 배열에서 제거
            while (i++ < g_client_count - 1) {
                g_client_socks[i - 1] = g_client_socks[i];
            }
            break;
        }
    }
    g_client_count--;
    printf("Client disconnected, Socket: %d\n", clnt_sock);
    pthread_mutex_unlock(&g_mutex);
    // --- 임계 영역 끝 ---

    close(clnt_sock);
    return NULL;
}

// 메시지 전송 함수 (echo 또는 broadcast)
void send_msg_to_all(char *msg, int len, int sender_sock) {
    pthread_mutex_lock(&g_mutex);
    if (g_broadcast_mode) { // Broadcast 모드
        for (int i = 0; i < g_client_count; i++) {
            write(g_client_socks[i], msg, len);
        }
    } else { // Echo 모드
        write(sender_sock, msg, len);
    }
    pthread_mutex_unlock(&g_mutex);
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}