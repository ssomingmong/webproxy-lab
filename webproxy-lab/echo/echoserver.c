#include "csapp.h"

// echo 함수 선언 (실제 구현은 echo.c에서)
// connfd를 받아서 클라이언트가 보낸 데이터를 그대로 돌려보냄
void echo(int connfd);

int main(int argc, char **argv) {
    int listenfd;   // 포트 열고 대기하는 fd (서버 시작 시 딱 한 번 생성)
    int connfd;     // 클라이언트 연결마다 새로 생기는 fd (Accept()가 반환)
    socklen_t clientlen;                    // 클라이언트 주소 구조체 크기
                                            // Accept()에 버퍼 크기 알려주는 용도
    struct sockaddr_storage clientaddr;     // 클라이언트 주소 정보 (바이너리)
                                            // IPv4/IPv6 둘 다 담을 수 있는 범용 구조체
    char client_hostname[MAXLINE];          // 클라이언트 IP 문자열 (예: "127.0.0.1")
    char client_port[MAXLINE];              // 클라이언트 포트 문자열 (예: "54321")

    // 서버는 포트번호만 받음: ./echoserver 8080
    // argv[0] = "./echoserver", argv[1] = "8080" → argc = 2
    if(argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    // Open_listenfd: 내부에서 socket() + bind() + listen() 한 방에 처리
    // 성공하면 클라이언트 대기 준비된 listenfd 반환
    listenfd = Open_listenfd(argv[1]);

    // 서버는 꺼지지 않고 계속 클라이언트를 받아야 하므로 무한루프
    while(1) {
        // 루프마다 초기화 필수!
        // Accept() 호출 후 clientlen이 실제 쓰여진 크기로 덮어씌워지기 때문
        clientlen = sizeof(struct sockaddr_storage);

        // Accept: 클라이언트 연결 올 때까지 블로킹
        // 연결 오면 connfd 반환 (클라이언트 전용 fd)
        // (SA *): sockaddr_storage → sockaddr 캐스팅 (Accept가 요구하는 타입)
        // &clientlen: 버퍼 크기 전달 + 실제 쓰여진 크기 돌려받음
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // Getnameinfo: clientaddr(바이너리 주소) → 문자열로 변환
        // client_hostname = "127.0.0.1", client_port = "54321"
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);

        // 연결된 클라이언트 정보 출력
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        // echo 함수 호출: 클라이언트가 보낸 데이터 그대로 돌려보냄
        echo(connfd);

        // 클라이언트 처리 끝나면 반드시 connfd 닫기
        // 안 닫으면 fd leak 발생 (프로세스당 최대 1024개)
        Close(connfd);
    }
}