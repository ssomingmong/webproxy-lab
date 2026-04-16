####################################################################
# CS:APP Proxy Lab
#
# 학생 소스 파일
####################################################################

이 디렉토리에는 CS:APP Proxy Lab 과제에 필요한 파일들이 있습니다.

proxy.c
csapp.h
csapp.c
    시작용 파일들입니다. csapp.c와 csapp.h는 교재에 설명되어 있습니다.

    이 파일들은 자유롭게 수정할 수 있습니다. 또한 추가 파일을 만들어
    제출할 수도 있습니다.

    프록시 서버나 tiny 서버에 사용할 고유 포트를 생성하려면
    `port-for-user.pl` 또는 `free-port.sh`를 사용하세요.

Makefile
    프록시 프로그램을 빌드하는 Makefile입니다. "make"를 입력하면
    빌드되고, 처음부터 다시 빌드하려면 "make clean" 후 "make"를
    실행하세요.

    "make handin"을 입력하면 제출용 tar 파일이 생성됩니다.
    원하는 대로 수정할 수 있습니다. 채점자는 이 Makefile을 이용해
    소스에서 프록시를 빌드합니다.

port-for-user.pl
    특정 사용자를 위한 랜덤 포트를 생성합니다.
    사용법: ./port-for-user.pl <userID>

free-port.sh
    프록시나 tiny에 사용할 수 있는 빈 TCP 포트를 찾아주는 스크립트입니다.
    사용법: ./free-port.sh

driver.sh
    기본(Basic), 동시성(Concurrency), 캐시(Cache) 항목의 자동 채점기입니다.
    사용법: ./driver.sh

nop-server.py
    자동 채점기의 보조 서버입니다.

tiny
    CS:APP 교재의 Tiny 웹 서버입니다.

