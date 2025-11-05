/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <string.h>

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

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
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

void doit(int fd) {
  size_t n;
  char buf[MAXLINE];
  rio_t rio;
  struct stat sbuf; // 파일 정보를 담을 구조체 sbuf 선언

  int line = 0;

  Rio_readinitb(&rio, fd);
  while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    if (line == 0) {
      char method[MAXLINE], uri[MAXLINE], version[MAXLINE], filename[MAXLINE], cgiargs[MAXLINE];
      
      strcpy(method, strtok(buf, " "));
      strcpy(uri, strtok(NULL, " "));
      strcpy(version, strtok(NULL, " "));

      parse_uri(uri, filename, cgiargs);

      if (strstr(filename, ".ico")) {
        printf("ico 요청 \n");
        return;
      }

      if (stat(filename, &sbuf) < 0) { // sbuf에 파일 정보 담기
        printf("error file: %s \n", filename);
        clienterror(fd, filename, "404", "Not found", "This file cannot be found");
        return;
      } 

      if (strlen(cgiargs) == 0) {
        printf("static file! %s \n", filename);
        serve_static(fd, filename, sbuf.st_size); // sbuf.st_size로 파일의 사이즈 전달
      } else {
        printf("dynamic file! %s, %s \n", filename, cgiargs);
        serve_dynamic(fd, filename, cgiargs);
      }
    } else {
      read_requesthdrs(&(rio));      
    }

    line++;
  }
}

void read_requesthdrs(rio_t *rp) {
  size_t n;
  char buf[MAXLINE];

  n = Rio_readlineb(rp, buf, MAXLINE);
  // printf("read_requests, %s \n", buf);
  return ;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
  char* tmp = strchr(uri, '?');

  strcpy(cgiargs, "");
  strcpy(filename, ".");

  if (tmp == NULL) { // 인자가 없는 경우 (아마 정적 컨텐츠?)
    if (strcmp(uri, "/") == 0) {
      strcat(filename, "/home.html");
    } else {
      strcat(filename, uri);
    }
  } else { // 인자가 있는 경우 (아마 동적 컨텐츠?)
    // strcat(filename, uri);
    *tmp = '\0';
    strcat(filename, uri);
    strcpy(cgiargs, tmp+1);
  }

  return 0;
};

void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXLINE];

  printf("serve_static: %s \n", filename);

  // 응답 라인 & 응답 헤더 작성
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Connection: close\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n", filesize);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
  Rio_writen(fd, buf, strlen(buf));

  // 응답 본체 작성
  if (strcmp(filetype, "image/ico") == 0) {
    printf("일단 내버려두자");
  } else {
    srcfd = Open(filename, O_RDONLY, 0); // 해당 파일 열고
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일 내용 복사한 뒤에 (srcp)
    close(srcfd); // 해당 파일 닫고
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize); // mmap 매핑 해제
  }
}

void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  } else if (strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  } else if (strstr(filename, ".ico")) {
    strcpy(filetype, "image/ico");
  } else {
    strcpy(filetype, "text/html");
  }

  printf("get_filetype: %s \n", filetype);
}


void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) {
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
  // Close(fd);

  /*
  1. adder 실행 파일을 수정하자
  2. 파일에서 adder.html을 열고, result-space 부분 찾기
  3. 찾은 부분에 결과값 삽입
  4. 클라이언트에 전송하기
  */

}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE] = "", body[MAXBUF] = "";

  // error.html 페이지에 에러 원인 추가
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body + strlen(body), "<body bgcolor=""ffffff"">\r\n");
  sprintf(body + strlen(body), "%s: %s\r\n", errnum, shortmsg);
  sprintf(body + strlen(body), "<p>%s: %s\r\n", longmsg, cause);
  sprintf(body + strlen(body), "<hr><em>The Tiny Web server</em>\r\n");

  // HTTP 응답 출력
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/*
doit => HTTP 트랜잭션 처리
clienterror => 오류 처리
read_requesthdrs => 요청 헤더를 읽는 함수 (별다른 동작 안하고 무시)
parse_uri => URI를 파싱해서 파일 이름과 인자들을 구분
get_filetype => file 이름을 받아서, file이 어떤 타입인지 구분 (정적 파일일때만 해당하는 거 같은데? 이미지인지, html인지 구분)
serve_static => 정적 컨텐츠 제공
serve_dynamic => 동적 컨텐츠 제공
*/

// do-it 로직 바꾸기, 첫 줄 읽고, 함수 안으로 들어가면 안됨 -> 반환 끝내고 소켓을 끊어버려서 이후의 값들을 못 읽음
// 전부다 읽은 다음에, 응답하는 함수 실행해야 함