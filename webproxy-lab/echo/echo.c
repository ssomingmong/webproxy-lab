#include "csapp.h"

void echo(int connfd) {
    size_t n;       // Rio_readlineb()가 반환하는 읽은 바이트 수
    char buf[MAXLINE];  // 클라이언트가 보낸 데이터 저장할 버퍼 (8192 bytes)
    rio_t rio;      // RIO 버퍼 구조체

    // Rio_readinitb: rio를 connfd와 연결 + 내부 버퍼 초기화
    // 이 호출 이후 rio는 connfd에서 데이터를 읽을 준비 완료
    Rio_readinitb(&rio, connfd);

    // Rio_readlineb: 클라이언트가 보낸 한 줄 읽기
    // 반환값 n = 읽은 바이트 수
    // n == 0 이면 EOF (클라이언트가 Close() 호출) → 루프 종료
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {

        // 서버 터미널에 몇 바이트 받았는지 출력
        // (int)n: size_t → int 캐스팅 (%d 포맷과 타입 맞추기 위해)
        printf("server received %d bytes\n", (int)n);

        // Rio_writen: 읽은 데이터를 그대로 클라이언트에게 전송 (echo!)
        // echoclient의 Rio_readlineb가 이 데이터를 받음
        Rio_writen(connfd, buf, n);
    }
}