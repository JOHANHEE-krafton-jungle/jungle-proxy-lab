#include "../csapp.h"

void echo(int connfd);

int main(int argc, char **argv) {
    int listenfd, connfd; // 듣기 소켓, 연결 소켓
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; // IPv4와 IPv6을 모두 담을 수 있는 사용자 타입
    char cilent_hostname[MAXLINE], cilent_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port> 인자값이 뭔가 이상해요 \n", argv[0]);
    }

    listenfd = Open_listenfd(argv[1]); // 서버 소켓 연결 가능한 상태로 설정

    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // accept 함수 실행, 클라이언트 요청이 listenfd에 도착하면 clientaddr에 클라이언트의 소켓 주소를 채운다.
        Getnameinfo((SA *)&clientaddr, clientlen, cilent_hostname, MAXLINE, cilent_port, MAXLINE, 0);
        printf("클라이언트 (%s %s)와 연결되었습니다. \n", cilent_hostname, cilent_port);
        echo(connfd); // 텍스트 줄을 읽고 출력하는 echo 함수
        Close(connfd);
    }
    exit(0);
}

void echo(int connfd) {
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    char tmp[] = "Response: ";

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("서버가 %d 바이트 크기만큼의 데이터를 받았습니다 \n", (int)n);
        Rio_writen(connfd, tmp, strlen(tmp));
        Rio_writen(connfd, buf, n);
    }
}