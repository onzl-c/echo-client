// echo-client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 1024
#define NAME_SIZE 20

void *send_msg(void *arg);
void *recv_msg(void *arg);
void error_handling(char *msg);

char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Syntax : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sprintf(name, "[CLIENT]"); // 클라이언트 이름 간단히 설정

    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void *thread_return;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("connect() error");
    } else {
        puts("Connected to server.........");
    }

    // 스레드 생성
    pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);
    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);

    // 스레드 종료 대기
    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);

    close(sock);
    return 0;
}

// 메시지 전송 스레드
void *send_msg(void *arg) {
    int sock = *((int *)arg);
    char name_msg[NAME_SIZE + BUF_SIZE];

    while (1) {
        fgets(msg, BUF_SIZE, stdin);
        if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")) {
            close(sock);
            exit(0);
        }
        sprintf(name_msg, "%s %s", name, msg);
        write(sock, name_msg, strlen(name_msg));
    }
    return NULL;
}

// 메시지 수신 스레드
void *recv_msg(void *arg) {
    int sock = *((int *)arg);
    char name_msg[NAME_SIZE + BUF_SIZE];
    int str_len;

    while (1) {
        str_len = read(sock, name_msg, NAME_SIZE + BUF_SIZE - 1);
        if (str_len <= 0) { // 서버 연결 끊김
            // main 스레드에서 close 하므로 여기서는 스레드만 종료
            return NULL;
        }
        name_msg[str_len] = 0;
        fputs(name_msg, stdout);
    }
    return NULL;
}

void error_handling(char *msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}