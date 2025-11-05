#include "../csapp.h"

int main(int argc, char **argv) {
/* 
    args: 전달받은 인자의 개수
    argv: 전달받은 인자 문자열들의 배열        
*/ 

    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio; // 출력용 내부 버퍼 관리자

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port> 인자 값이 뭔가 이상해요 \n", argv[0]); // stderr 버퍼 (표준 오류 버퍼)에 뒤의 메시지를 추가
        exit(0);
    }

    host = argv[1]; // 호스트 이름
    port = argv[2]; // 포트 번호

    clientfd = open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd); // rio 버퍼 초기화

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf)); // buf의 길이만큼 write를 반복
        Rio_readlineb(&rio, buf, MAXLINE); // 내부 버퍼를 이용해서 정확히 한 줄을 읽어옴
        Fputs(buf, stdout);
    }
    Close(clientfd); // clientfd 값으로 열었던 파일을 닫기
    exit(0);
}