## demo 
![Image](https://github.com/user-attachments/assets/981a511a-0c80-425f-8c56-d5dc9c35239b)

## 컴파일
- 서버 프로그램 컴파일: `gcc echo-server.c -o echo-server -pthread`
- 클라이언트 프로그램 컴파일: `gcc echo-client.c -o echo-client -pthread`

## 동시성과 동기화 구현
- 동시성(concurrency) → multi-threading으로 구현
    - 여러 client의 요청을 동시에 처리하거나, 메시지 송신과 수신을 동시에 진행하는 것
- 동기화(synchronization) → mutex로 구현
    - 여러 thread가 공동의 자원에 동시 접근하여 생기는 문제를 막는 것

### echo-server

> 여러 client의 연결을 끊김 없이 받아들이고, 각 클라이언트의 메시지를 독립적으로 처리하는 것
> 
1. 접속 담당 thread와 통신 담당 thread의 분리(역할 분담)
    1. main: 새로운 클라이언트가 접속하는지 계속 listen, 접속하면 accept
        
        ```c
        // main 함수 내부: 접속만 담당한다.
        while (1) {
            clnt_sock = accept(serv_sock, ...); // 새로운 클라이언트 접속!
            ...
            // "이 클라이언트는 네가 맡아!"라며 새 스레드를 만들고 일을 넘김
            pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);
            pthread_detach(t_id);
        }
        ```
        
    2. `handle_clnt`: 이미 접속된 클라이언트들과 실제로 메시지를 주고받음
2. 공유 자원 보호를 위한 잠금 장치(`mutex`)
    
    여러 클라이언트 스레드가 동시에 접속자 목록(`g_client_socks`)을 수정하거나, 서버 콘솔에 메시지를 출력하려고 하면 데이터가 꼬일 수 있음 → `pthread_mutex_t` 사용
    
    1. 공유 자원에 접근하기 전, 반드시 `pthread_mutex_lock(&g_mutex);`을 호출해 lock
    2. lock 상태라면 다른 thread는 대기
    3. 공유 자원을 다 사용하면 `pthread_mutex_unlock(&g_mutex);`으로 문을 열어 다른 스레드가 사용할 수 있도록 함
    
    ```c
    // handle_clnt 함수 내부: 메시지 전송 로직
    void send_msg_to_all(...) {
        pthread_mutex_lock(&g_mutex); // 문 잠그기!
        if (g_broadcast_mode) {
            // 모든 클라이언트에게 메시지 전송
            for (int i = 0; i < g_client_count; i++) {
                write(g_client_socks[i], msg, len);
            }
        } else {
            // 특정 클라이언트에게만 전송
            write(sender_sock, msg, len);
        }
        pthread_mutex_unlock(&g_mutex); // 문 열기!
    }
    ```
    

### echo-client

> 내가 메시지를 입력하는 중에도 서버가 보내는 메시지를 실시간으로 받을 수 있게 하는 것
> 
- 송신 thread와 수신 thread의 분리
    - thread가 하나라면?
        
        키보드 입력을 기다리는 `fgets()` 함수나 서버 메시지를 기다리는 `read()` 함수에서 프로그램이 멈출 것(blocking)
        
        → 내가 메시지를 입력하는 동안 서버가 보낸 메시지를 볼 수 없고, 서버 메시지를 기다리는 동안 내가 메시지를 입력할 수 없을 것 
        
    1. `send_msg`: 키보드 입력(`fgets`)을 받아 서버로 메시지를 보내는(`write`) 일 전담
    2. `recv_msg`: 서버로부터 메시지가 오는지 항상 `read`, 메시지가 오면 화면에 출력(`fputs`)하는 일만 전담
    
    ```c
    // main 함수 내부
    // "너는 보내기만 해", "너는 받기만 해"라며 두 스레드를 생성
    pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);
    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
    
    // 두 스레드가 끝날 때까지 대기
    pthread_join(snd_thread, ...);
    pthread_join(rcv_thread, ...);
    ```