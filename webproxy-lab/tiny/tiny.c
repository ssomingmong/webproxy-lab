/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

void doit(int fd) {
  int is_static;          // parse_uri() 반환값
                          // 1 = 정적 콘텐츠 (파일 그대로 전송)
                          // 0 = 동적 콘텐츠 (CGI 프로그램 실행)
  struct stat sbuf;       // stat() 시스템콜이 채워주는 파일 정보 구조체
                          // sbuf.st_size = 파일 크기 (bytes)
                          // sbuf.st_mode = 파일 타입 + 권한 비트
  char buf[MAXLINE];      // 네트워크에서 읽은 요청 라인을 저장하는 버퍼
  char method[MAXLINE];   // 요청 라인의 첫 번째 토큰: "GET", "POST" 등
  char uri[MAXLINE];      // 요청 라인의 두 번째 토큰: "/index.html", "/cgi-bin/adder?1&2"
  char version[MAXLINE];  // 요청 라인의 세 번째 토큰: "HTTP/1.0", "HTTP/1.1"
  char filename[MAXLINE]; // parse_uri()가 채워주는 실제 파일 경로
                          // "/index.html" → "./index.html"
  char cgiargs[MAXLINE];  // parse_uri()가 채워주는 CGI 인자
                          // "/cgi-bin/adder?15000&213" → "15000&213"
                          // 정적 콘텐츠면 빈 문자열 ""
  rio_t rio;              // RIO 버퍼 구조체
                          // fd에서 데이터를 읽을 때 내부 버퍼(8192byte)로 사용

  // fd를 rio와 연결 + 내부 버퍼 초기화
  // 이 호출 이후부터 Rio_readlineb로 fd에서 읽기 가능
  Rio_readinitb(&rio, fd);

  // 요청의 첫 번째 줄(요청 라인)을 buf에 저장
  // 예: "GET /index.html HTTP/1.0\r\n"
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);

  // sscanf: buf에서 공백 기준으로 세 개의 토큰을 추출
  // "GET /index.html HTTP/1.0\r\n"
  //  ↓           ↓          ↓
  // method      uri       version
  sscanf(buf, "%s %s %s", method, uri, version);

  // Tiny는 GET 메서드만 지원
  // strcasecmp: 대소문자 구분 없이 문자열 비교
  //   반환값 0   = 두 문자열이 같음 (GET 맞음)
  //   반환값 비0 = 두 문자열이 다름 (GET 아님) → 501 에러 후 함수 종료
  if(strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this metdhod");
    return;
  }

  // 요청 라인 다음에 오는 헤더들을 읽고 버림
  // 예: "Host: localhost:8080\r\n", "User-Agent: ...\r\n", "\r\n"(빈 줄=헤더 끝)
  // Tiny는 헤더 내용을 사용하지 않지만
  // 읽지 않으면 소켓 버퍼에 남아서 이후 통신에 문제가 생김
  read_requesthdrs(&rio);

  // URI를 파싱해서 filename과 cgiargs를 채워줌
  // 정적: uri="/index.html"          → filename="./index.html", cgiargs="",         반환값=1
  // 동적: uri="/cgi-bin/adder?1&2"   → filename="./cgi-bin/adder", cgiargs="1&2",  반환값=0
  is_static = parse_uri(uri, filename, cgiargs);

  // stat(): filename에 해당하는 파일 정보를 sbuf에 저장
  // 반환값 < 0: 파일이 존재하지 않음 → 404 에러 후 함수 종료
  if(stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if(is_static) {
    // S_ISREG(sbuf.st_mode): 일반 파일인지 확인 (디렉토리, 소켓 등이면 false)
    // S_IRUSR & sbuf.st_mode: 소유자 읽기 권한이 있는지 확인
    // 둘 중 하나라도 만족 못하면 → 403 에러 후 함수 종료
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbiddn", "Tiny couldn't read the file");
      return;
    }
    // sbuf.st_size: 파일 크기(bytes)를 serve_static에 넘겨줌
    // Content-Length 헤더 값으로 사용됨
    serve_static(fd, filename, sbuf.st_size);
  }
  else {
    // S_ISREG(sbuf.st_mode): 일반 파일인지 확인
    // S_IXUSR & sbuf.st_mode: 소유자 실행 권한이 있는지 확인
    // 둘 중 하나라도 만족 못하면 → 403 에러 후 함수 종료
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

/*
 * clienterror - 클라이언트에게 HTTP 에러 응답을 전송하는 함수
 *
 * 매개변수:
 *   fd       : 클라이언트와 연결된 소켓 fd (이 fd로 응답을 전송)
 *   cause    : 에러의 원인 (예: 요청한 파일명, 지원하지 않는 메서드명)
 *   errnum   : HTTP 상태 코드 문자열 (예: "404", "403", "501")
 *   shortmsg : 상태 코드에 대한 짧은 설명 (예: "Not Found", "Forbidden")
 *   longmsg  : 에러에 대한 더 자세한 설명 (예: "Tiny couldn't find this file")
 *
 * 동작 방식:
 *   1. HTML로 된 에러 페이지(body)를 문자열로 만든다
 *   2. HTTP 응답 헤더를 한 줄씩 전송한다
 *   3. 만들어둔 HTML body를 전송한다
 *
 * 호출 예시 (doit에서):
 *   clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
 *
 *   → 클라이언트 브라우저에 아래와 같은 HTTP 응답이 전달됨:
 *
 *   HTTP/1.0 404 Not found\r\n
 *   Content-type: text/html\r\n
 *   Content-length: 123\r\n
 *   \r\n
 *   <html><title>Tiny Error</title>
 *   <body bgcolor="ffffff">
 *   404: Not found
 *   <p>Tiny couldn't find this file: ./index.html
 *   <hr><em>The Tiny Web Server</em>
 */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE]; // HTTP 헤더 한 줄을 임시로 담는 버퍼
                       // sprintf로 한 줄 만든 뒤 Rio_writen으로 바로 전송하고 재사용함
    char body[MAXBUF]; // HTML 응답 본문 전체를 누적해서 담는 버퍼
                       // MAXBUF = 8192 bytes (csapp.h에 정의됨)

    /* ---------------------------------------------------------------
     * [1단계] HTML 응답 본문(body) 만들기
     *
     * sprintf(body, "%s...", body, ...) 패턴을 반복해서
     * body 문자열 뒤에 HTML을 계속 이어붙이는 방식
     *
     * 주의: sprintf(body, "...", body)는 입력과 출력이 같은 버퍼라서
     *       원래는 undefined behavior이지만,
     *       csapp 교재 2019 업데이트에서 이 방식으로 수정됨
     *       (이전엔 strcat을 썼지만 aliasing 문제 때문에 변경)
     * --------------------------------------------------------------- */

    // HTML 문서 시작 + 탭 제목 설정
    // 결과: "<html><title>Tiny Error</title>"
    sprintf(body, "<html><title>Tiny Error</title>");

    // body 배경색 흰색(ffffff)으로 설정
    // 결과: "<html>...<body bgcolor="ffffff">\r\n"
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);

    // 에러 코드와 짧은 설명 출력
    // 예: "404: Not found\r\n"
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);

    // 자세한 설명 + 원인(파일명 or 메서드명) 출력
    // 예: "<p>Tiny couldn't find this file: ./index.html\r\n"
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);

    // 하단에 수평선 + 서버 이름 표시
    // 예: "<hr><em>The Tiny Web Server</em>\r\n"
    sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);

    /* ---------------------------------------------------------------
     * [2단계] HTTP 응답 헤더 전송
     *
     * HTTP 응답 형식:
     *   상태 라인\r\n
     *   헤더1\r\n
     *   헤더2\r\n
     *   \r\n          ← 빈 줄: 헤더 끝을 나타냄
     *   본문
     *
     * buf에 한 줄씩 만들어서 바로 전송
     * Rio_writen(fd, buf, strlen(buf)): buf의 내용을 fd에 strlen(buf) 바이트 전송
     * --------------------------------------------------------------- */

    // 상태 라인 전송: "HTTP/1.0 404 Not found\r\n"
    // HTTP 버전 1.0 고정 (Tiny는 1.0만 지원)
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));

    // Content-type 헤더 전송: "Content-type: text/html\r\n"
    // 브라우저에게 응답 본문이 HTML임을 알려줌
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));

    // Content-length 헤더 전송 + 헤더 종료 빈 줄
    // "Content-length: 123\r\n\r\n"
    // strlen(body): 실제 HTML body의 바이트 수
    // (int) 캐스팅: strlen은 size_t(unsigned) 반환 → %d와 타입 맞추기 위해
    // \r\n\r\n: 마지막 헤더 뒤 빈 줄로 헤더 블록 종료 (이 뒤부터 본문)
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));

    /* ---------------------------------------------------------------
     * [3단계] HTTP 응답 본문(HTML) 전송
     *
     * 위에서 만들어둔 body 문자열 전체를 fd에 전송
     * 클라이언트(브라우저)는 이 HTML을 받아서 에러 페이지를 렌더링함
     * --------------------------------------------------------------- */
    Rio_writen(fd, body, strlen(body));
}

/*
 * read_requesthdrs - HTTP 요청 헤더를 읽고 버리는 함수
 *
 * 매개변수:
 *   rp : rio 버퍼 구조체 포인터 (이미 doit에서 Rio_readinitb로 초기화된 상태)
 *
 * 동작:
 *   HTTP 요청에서 요청 라인 다음에 오는 헤더들을 한 줄씩 읽는다.
 *   빈 줄("\r\n")이 나오면 헤더가 끝난 것이므로 루프를 종료한다.
 *   Tiny는 헤더 내용을 사용하지 않지만, 읽지 않으면 소켓 버퍼에
 *   데이터가 남아 이후 통신에 문제가 생기기 때문에 반드시 읽어야 한다.
 *
 * HTTP 요청 구조:
 *   GET /index.html HTTP/1.0\r\n      ← 요청 라인 (doit에서 이미 읽음)
 *   Host: localhost:8080\r\n          ┐
 *   User-Agent: Mozilla/5.0\r\n       │ 이 함수가 읽고 버리는 영역
 *   Accept: text/html\r\n             ┘
 *   \r\n                              ← 빈 줄: 헤더 끝 신호 → 루프 종료
 */
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE]; // 헤더 한 줄을 읽어서 임시로 저장하는 버퍼

  // 헤더 첫 번째 줄을 읽어서 buf에 저장
  // doit에서 요청 라인은 이미 읽었으므로 여기서는 그 다음 줄부터 읽힘
  Rio_readlineb(rp, buf, MAXLINE);

  // strcmp(buf, "\r\n") != 0 이면 아직 헤더가 끝나지 않은 것
  // 빈 줄("\r\n")이 오면 strcmp가 0을 반환 → 루프 종료
  while(strcmp(buf, "\r\n")) {
    // 다음 헤더 줄을 읽어서 buf에 덮어씀 (이전 줄은 버려짐)
    Rio_readlineb(rp, buf, MAXLINE);

    // 읽은 헤더를 서버 터미널에 출력 (디버깅용)
    // 실제로 이 값을 사용하지는 않음
    printf("%s", buf);
  }
  return;
}

/*
 * parse_uri - URI를 분석해서 파일 경로(filename)와 CGI 인자(cgiargs)를 추출하는 함수
 *
 * 매개변수:
 *   uri      : 클라이언트가 요청한 URI (예: "/index.html", "/cgi-bin/adder?1&2")
 *   filename : 추출한 실제 파일 경로를 저장할 버퍼 (호출자가 선언한 배열)
 *   cgiargs  : 추출한 CGI 인자를 저장할 버퍼 (호출자가 선언한 배열)
 *
 * 반환값:
 *   1 = 정적 콘텐츠 ("cgi-bin" 없음)
 *   0 = 동적 콘텐츠 ("cgi-bin" 있음)
 *
 * URI 변환 예시:
 *   정적: "/index.html"          → filename="./index.html",    cgiargs=""
 *   정적: "/"                    → filename="./home.html",     cgiargs=""
 *   동적: "/cgi-bin/adder?1&2"   → filename="./cgi-bin/adder", cgiargs="1&2"
 *   동적: "/cgi-bin/adder"       → filename="./cgi-bin/adder", cgiargs=""
 */
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr; // URI 안에서 '?'의 위치를 가리키는 포인터

  /* ---------------------------------------------------------------
   * [정적 콘텐츠] URI에 "cgi-bin"이 없는 경우
   *
   * strstr(uri, "cgi-bin"): uri 안에서 "cgi-bin" 문자열을 탐색
   *   - 발견되면 해당 위치의 포인터 반환 (true)
   *   - 없으면 NULL 반환 (false)
   * !strstr(...) == true → "cgi-bin" 없음 → 정적 콘텐츠
   * --------------------------------------------------------------- */
  if(!strstr(uri, "cgi-bin")) {
    // 정적 콘텐츠는 CGI 인자가 없으므로 빈 문자열로 초기화
    strcpy(cgiargs, "");

    // filename = "." + uri
    // strcpy로 "."을 먼저 복사한 뒤, strcat으로 uri를 이어붙임
    // 예: uri="/index.html" → filename="./index.html"
    strcpy(filename, ".");
    strcat(filename, uri);  // ※ cgiargs가 아닌 filename에 붙여야 함

    // uri의 마지막 문자가 '/'이면 디렉토리 요청
    // 기본 페이지(home.html)로 연결
    // 예: uri="/" → filename="./" + "home.html" = "./home.html"
    if(uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");

    return 1; // 정적 콘텐츠임을 doit()에 알림
  }

  /* ---------------------------------------------------------------
   * [동적 콘텐츠] URI에 "cgi-bin"이 있는 경우
   *
   * URI 구조: "/cgi-bin/adder?15000&213"
   *                           ↑
   *                     '?' 이전 = CGI 프로그램 경로
   *                     '?' 이후 = CGI에 넘길 인자
   * --------------------------------------------------------------- */
  else {
    // index(uri, '?'): uri에서 '?' 문자의 위치를 찾아 포인터 반환
    // '?'가 없으면 NULL 반환
    ptr = index(uri, '?');

    if(ptr) {
      // ptr+1: '?' 바로 다음 문자부터 끝까지 = CGI 인자
      // 예: "/cgi-bin/adder?15000&213" → cgiargs="15000&213"
      strcpy(cgiargs, ptr + 1);

      // *ptr = '\0': '?' 위치를 문자열 종료 문자로 덮어써서 uri를 잘라냄
      // 예: "/cgi-bin/adder?15000&213" → "/cgi-bin/adder\0"
      // 이후 strcat(filename, uri)에서 '?' 이전 경로만 붙음
      *ptr = '\0';
    }
    else
      // '?'가 없으면 CGI 인자 없음
      // 예: "/cgi-bin/adder" → cgiargs=""
      strcpy(cgiargs, "");

    // filename = "." + uri ('?' 이후가 잘린 상태)
    // 예: uri="/cgi-bin/adder" → filename="./cgi-bin/adder"
    strcpy(filename, ".");
    strcat(filename, uri);

    return 0; // 동적 콘텐츠임을 doit()에 알림
  }
}

/*
 * serve_static - 정적 파일을 읽어서 클라이언트에게 HTTP 응답으로 전송하는 함수
 *
 * 매개변수:
 *   fd       : 클라이언트와 연결된 소켓 fd
 *   filename : 전송할 파일 경로 (예: "./index.html")
 *   filesize : 전송할 파일 크기 (bytes), doit()에서 stat()으로 구해서 넘겨줌
 *
 * 동작 2단계:
 *   [1단계] HTTP 응답 헤더를 buf에 만들어서 전송
 *   [2단계] 파일을 메모리에 매핑(mmap)한 뒤 그대로 전송
 *
 * 클라이언트가 받는 최종 응답:
 *   HTTP/1.0 200 OK\r\n
 *   Server: Tiny Web Server\r\n
 *   Connection: close\r\n
 *   Content-length: 1234\r\n
 *   Content-type: text/html\r\n
 *   \r\n
 *   (파일 내용)
 */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;                          // 파일을 열었을 때 반환되는 파일 디스크립터
    char *srcp;                         // mmap()이 반환하는 포인터
                                        // 파일 내용이 매핑된 메모리 주소를 가리킴
    char filetype[MAXLINE];             // get_filetype()이 채워주는 MIME 타입 문자열
                                        // 예: "text/html", "image/png"
    char buf[MAXBUF];                   // HTTP 응답 헤더 전체를 누적해서 담는 버퍼

    /* ---------------------------------------------------------------
     * [1단계] HTTP 응답 헤더 전송
     *
     * get_filetype(): 파일 확장자를 보고 MIME 타입을 결정
     *   .html → "text/html"
     *   .png  → "image/png"
     *   기타  → "text/plain"
     *
     * sprintf로 헤더를 buf에 한 줄씩 누적한 뒤 한 번에 전송
     * 마지막 \r\n\r\n: Content-type 헤더 뒤 빈 줄로 헤더 블록 종료
     * --------------------------------------------------------------- */

    // filename의 확장자를 보고 MIME 타입을 filetype에 저장
    // 예: "./index.html" → filetype = "text/html"
    get_filetype(filename, filetype);

    // 응답 헤더를 buf에 누적
    sprintf(buf, "HTTP/1.0 200 OK\r\n");                          // 상태 라인: 성공
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);           // 서버 이름
    sprintf(buf, "%sConnection: close\r\n", buf);                 // 응답 후 연결 종료
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);      // 파일 크기 (bytes)
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);    // MIME 타입 + 헤더 종료

    // 완성된 헤더를 클라이언트에게 전송
    Rio_writen(fd, buf, strlen(buf));

    // 전송한 헤더를 서버 터미널에 출력 (디버깅용)
    printf("Response headers:\n");
    printf("%s", buf);

    /* ---------------------------------------------------------------
     * [2단계] 파일 본문 전송
     *
     * open() → mmap() → close() → Rio_writen() → munmap() 순서
     *
     * mmap()을 쓰는 이유:
     *   파일을 read()로 읽으면 커널 버퍼 → 사용자 버퍼 → 소켓 버퍼로
     *   2번 복사가 일어남. mmap()은 파일을 메모리에 직접 매핑해서
     *   커널 버퍼 → 소켓 버퍼로 1번만 복사하므로 더 효율적임
     * --------------------------------------------------------------- */

    // 파일을 읽기 전용(O_RDONLY)으로 열어서 srcfd 획득
    srcfd = Open(filename, O_RDONLY, 0);

    // mmap(): 파일을 가상 메모리 공간에 매핑하고 시작 주소를 srcp에 저장
    // 인자 설명:
    //   0            : 커널이 매핑 주소를 자동으로 선택
    //   filesize     : 매핑할 크기 (파일 전체)
    //   PROT_READ    : 읽기 전용으로 매핑
    //   MAP_PRIVATE  : 이 프로세스만 사용하는 private 매핑 (쓰기 시 복사본 생성)
    //   srcfd        : 매핑할 파일의 fd
    //   0            : 파일의 시작(offset 0)부터 매핑
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);

    // 파일 fd는 mmap 이후 더 이상 필요 없으므로 즉시 닫음
    // mmap으로 이미 메모리에 매핑됐기 때문에 fd를 닫아도 데이터는 유지됨
    Close(srcfd);

    // 매핑된 메모리(srcp)의 내용을 클라이언트 fd에 filesize 바이트 전송
    // 브라우저는 이 데이터를 받아서 렌더링함
    Rio_writen(fd, srcp, filesize);

    // 사용이 끝난 메모리 매핑 해제
    // 해제하지 않으면 프로세스가 살아있는 동안 메모리를 계속 점유함
    Munmap(srcp, filesize);
}

/*
 * get_filetype - 파일 확장자를 보고 MIME 타입을 결정하는 함수
 *
 * 매개변수:
 *   filename : 파일 경로 (예: "./index.html")
 *   filetype : MIME 타입을 저장할 버퍼 (호출자가 선언한 배열)
 *              이 함수가 결과를 채워줌
 *
 * MIME 타입이란?
 *   브라우저에게 "이 파일이 어떤 종류인지" 알려주는 문자열.
 *   Content-type 헤더에 담겨 전송되며, 브라우저는 이걸 보고
 *   HTML이면 렌더링, 이미지면 표시, 그 외엔 텍스트로 표시한다.
 *
 * 확장자 → MIME 타입 매핑:
 *   .html → text/html
 *   .gif  → image/gif
 *   .png  → image/png
 *   .jpg  → image/jpeg
 *   그 외 → text/plain
 */
void get_filetype(char *filename, char *filetype)
{
    // strstr(filename, ".html"): filename 안에 ".html" 문자열이 있는지 탐색
    // 있으면 해당 위치 포인터 반환(true), 없으면 NULL(false)
    if(strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if(strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if(strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if(strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        // 알 수 없는 확장자는 일반 텍스트로 처리
        strcpy(filetype, "text/plain");
}

