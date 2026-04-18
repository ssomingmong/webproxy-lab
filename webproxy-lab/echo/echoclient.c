/*
<echoclient가 하는일>

사용자가 터미널에 타이핑
        ↓
  echoclient가 서버로 전송  →  [echo server]
                           ←  그대로 돌려보냄
        ↓
  터미널에 출력

  이 흐름을 코드 구조로 바꾸면:

  1. 서버에 연결        Open_clientfd()
  2. RIO 초기화        Rio_readinitb()
  3. 루프:
        - stdin에서 읽기    Fgets()
        - 서버로 전송       Rio_writen()
        - 서버 응답 읽기    Rio_readlineb()
        - 화면에 출력       Fputs()
  4. 연결 종료              Close()
*/

#include "csapp.h"  // 시스템 헤더(<>)가 아닌 로컬 파일("")로 포함
                    // 소켓 함수, RIO 함수, rio_t, MAXLINE 등이 모두 여기에 정의됨

int main(int argc, char **argv) {
    // argc = argument count: 커맨드라인 인자 개수
    // argv = argument vector: 커맨드라인 인자 배열
    // 실행 예시: ./echoclient localhost 8080
    //   argv[0] = "./echoclient" (프로그램 이름)
    //   argv[1] = "localhost"    (서버 호스트명)
    //   argv[2] = "8080"         (포트번호)
    //   argc    = 3

    int clientfd;           // 서버와 연결된 소켓 fd 번호 (예: 3, 4, 5...)
    char *host, *port;      // argv[1], argv[2]를 가리킬 포인터
    char buf[MAXLINE];      // 데이터 송수신용 버퍼 (MAXLINE = 8192 bytes)
    rio_t rio;              // RIO 버퍼 구조체
                            // 내부에 fd, 버퍼 8192byte, 읽을 위치 등을 들고 있음
                            // Rio_readinitb()로 초기화한 뒤에 사용 가능

    // 인자가 3개가 아니면 잘못 실행한 것 → 에러 메시지 출력 후 종료
    // stderr: 에러 전용 출력 채널 (stdout과 분리되어 파일로 리다이렉션해도 터미널에 출력됨)
    // argv[0]: 프로그램 이름 자체를 에러 메시지에 포함
    if(argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }

    host = argv[1];  // "localhost"
    port = argv[2];  // "8080"

    // Open_clientfd: 내부에서 socket() + connect() + 3-way handshake까지 처리
    // 성공하면 서버와 연결된 소켓 fd 반환 → clientfd에 저장
    clientfd = Open_clientfd(host, port);

    // Rio_readinitb: rio 구조체를 clientfd와 연결하고 내부 버퍼 초기화
    // &rio: rio 구조체의 주소를 넘겨야 함수 내부에서 원본 값을 바꿀 수 있음
    // 이 호출 이후 rio는 clientfd에서 데이터를 읽을 준비 완료
    Rio_readinitb(&rio, clientfd);

    // Fgets: 키보드(stdin)에서 한 줄 읽어서 buf에 저장
    // 반환값이 NULL이면 Ctrl+D (EOF) 입력 → 루프 종료
    while(Fgets(buf, MAXLINE, stdin) != NULL) {

        // Rio_writen: buf 내용을 clientfd를 통해 서버로 전송
        // strlen(buf): 실제 데이터 길이만큼만 전송 (MAXLINE 쓰면 쓰레기값까지 전송)
        Rio_writen(clientfd, buf, strlen(buf));

        // Rio_readlineb: 서버가 echo해서 돌려보낸 응답을 buf에 저장
        // '\n' 나올 때까지 한 줄 읽음
        // Rio_readinitb(초기화)와 이름이 비슷하지만 전혀 다른 함수!
        Rio_readlineb(&rio, buf, MAXLINE);

        // Fputs: buf 내용을 화면(stdout)에 출력
        // printf(buf) 대신 Fputs 쓰는 이유: buf에 %d 같은 게 들어오면 보안 취약점 발생
        Fputs(buf, stdout);
    }

    Close(clientfd);
    exit(0);
}