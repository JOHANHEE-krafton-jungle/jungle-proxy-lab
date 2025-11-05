#include <stdio.h>
#include "csapp.h"

typedef struct _cache_list {
  struct _cache_node *tail;
  struct _cache_node *head;
  unsigned int total_size;
} Cache_List;

typedef struct _cache_node {
  struct _cache_node *prev;
  struct _cache_node *next;
  char *key;
  char *content;
  char *type;
  unsigned int size;
} Cache_Node;

void proxy_response(int fd, char *server_hostname, char *server_port, char *filename, char **request_headers);
void proxy_request(int connfd, int serverfd, char *server_hostname, char *server_port, char *filename, char **request_headers);
void send_response_with_cache(int connfd, Cache_Node *cache_ptr, char **request_headers);
void handle_requestlines(rio_t *rp, char *server_hostname, char *server_port, char *filename);
void handle_requestheaders(rio_t *rp, char **request_headers);
void parse_uri(char *uri, char *hostname, char *port, char *filename);
void *thread(void *argp);

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static Cache_List cache_list;

int main(int argc, char**argv)
{
  if (argc != 2) {
    fprintf(stderr, "인자 값이 잘못된듯?\n");
    return 1;
  }

  int listenfd, *connfd;
  struct sockaddr_storage clientaddr; // 클라이언트의 주소를 저장하기 위해 선언 (IPv4와 IPv6을 모두 담을 수 있는 사용자 타입)
  socklen_t clientlen;
  pthread_t tid;
  char cilent_hostname[MAXLINE]="", cilent_port[MAXLINE]=""; // 클라이언트의 호스트 이름 & 포트 번호

  listenfd = Open_listenfd(argv[1]);

  while (1) {
    clientlen = sizeof(struct sockaddr_storage);
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, cilent_hostname, MAXLINE, cilent_port, MAXLINE, 0);
    fprintf(stderr, "proxy: 클라이언트 (%s, %s)와 연결되었습니다. \n", cilent_hostname, cilent_port);
    Pthread_create(&tid, NULL, thread, connfd);
  }

  Close(listenfd);
  return 0;
}

void *thread(void *argp) {
  char server_hostname[MAXLINE]="", server_port[MAXLINE]="", filename[MAXLINE]="/"; // 서버의 호스트 이름 & 포트 번호 & 요청한 파일 이름
  char *request_headers[100]; // 클라이언트가 보낸 요청 헤더들을 저장한 배열
  int serverfd, connfd = *((int *)argp);

  Pthread_detach(pthread_self());
  Free(argp);
  
  proxy_response(connfd, server_hostname, server_port, filename, request_headers); // 파싱 처리해서 호스트 이름이랑 포트 번호랑 다 받아오고

  fprintf(stderr, "클라이언트 요청 다 파싱했습니다. server_hostname: %s, server_port: %s, filename: %s \n", server_hostname, server_port, filename);

  // 캐시 검사
  // Cache_Node *node = cache_list.head;
  // Cache_Node *cache_hit_ptr = NULL;

  // char *tmp_key = (char *)calloc(1, (size_t)(strlen(server_hostname) + strlen(server_port) + strlen(filename) + 1));
  // strcat(tmp_key, server_hostname);
  // strcat(tmp_key, server_port); 
  // strcat(tmp_key, filename);

  // while (node != NULL) {
  //   if (strcmp(node->key, tmp_key) == 0) {
  //     cache_hit_ptr = node;
  //     break;
  //   }
  //   node = node->next;
  // }

  // free(tmp_key);

  // if (cache_hit_ptr != NULL) {
  //   fprintf(stderr, "cache hit!\n");
  //   send_response_with_cache(connfd, cache_hit_ptr, request_headers);
  // } else {
  //   fprintf(stderr, "cache miss!\n");
  //   serverfd = Open_clientfd(server_hostname, server_port); // 클라이언트 소켓 연 다음에
  //   proxy_request(connfd, serverfd, server_hostname, server_port, filename, request_headers); // 여기서 요청 전송한 다음에, 받고 다시 클라이언트에게 전송하는 것까지
  // }

  serverfd = Open_clientfd(server_hostname, server_port); // 클라이언트 소켓 연 다음에
  proxy_request(connfd, serverfd, server_hostname, server_port, filename, request_headers); // 여기서 요청 전송한 다음에, 받고 다시 클라이언트에게 전송하는 것까지

  memset(filename, 0, MAXLINE);
  strcat(filename, "/");
  Close(connfd);

  return NULL;
}

void proxy_response(int fd, char *server_hostname, char *server_port, char *filename, char **request_headers) {
  /*
  클라이언트로부터 받은 요청을 처리하는 함수

  Args: 
    fd = 연결 소켓 식별자

  Returns:
    server_hostname = 클라이언트가 요청한 서버 호스트 이름
    server_port = 클라이언트가 요청한 서버 포트 번호
    server_hostname = 클라이언트가 요청한 요청 헤더들
  */
  rio_t rio;

  Rio_readinitb(&rio, fd);
  handle_requestlines(&rio, server_hostname, server_port, filename);
  handle_requestheaders(&rio, request_headers);
}

void handle_requestlines(rio_t *rp, char *server_hostname, char *server_port, char *filename) {
  /*
  HTTP 요청 라인을 처리하는 함수

  Args: 
    rp = rio_t 버퍼 포인터
    
  Returns:
    server_hostname = 클라이언트가 요청한 서버 호스트 이름
    server_port = 클라이언트가 요청한 서버 포트 번호
    server_hostname = 클라이언트가 요청한 요청 헤더들
  */

  char buf[MAXLINE]="";
  Rio_readlineb(rp, buf, MAXLINE);

  strtok(buf, " ");  
  char *uri = strtok(NULL, " ");
  strtok(NULL, " ");

  // method랑 version은 버리고, uri만 추가로 파싱
  parse_uri(uri, server_hostname, server_port, filename);
  fprintf(stderr, "After parsing hostname: %s, port: %s, filename: %s\n", server_hostname, server_port, filename);
}

void parse_uri(char *uri, char *hostname, char *port, char *filename) {
  /*
  HTTP 요청 라인을 파싱하는 함수
  uri를 파싱해서, hostname, port, filename 문자열의 값을 채운다.

  Args:
    uri = 파싱할 uri 배열(의 포인터)

  Returns:
    server_hostname = 클라이언트가 요청한 서버 호스트 이름
    server_port = 클라이언트가 요청한 서버 포트 번호
    server_hostname = 클라이언트가 요청한 요청 헤더들  
  */  
  char *tmp1 = strchr(uri, '/');
  *tmp1 = '\0';
  char *tmp2 = strchr(tmp1+1, '/');
  *tmp2 = '\0';

  char *tmp_filename_p = strchr(tmp2+1, '/');
  *tmp_filename_p = '\0';
  strcat(filename, tmp_filename_p+1); // filename 파싱 및 복사해놓기
  
  char *tmp3 = strchr(tmp2+1, ':'); // 포트 번호 있는지 찾기

  if (tmp3 == NULL) { // 포트 번호 미포함
    strcpy(hostname, tmp2+1);
  } else {
    *tmp3 = '\0'; // 포트 번호 포함
    strcpy(hostname, tmp2+1); 
    strcpy(port, tmp3+1);
  }
}

void handle_requestheaders(rio_t *rp, char **request_headers) {
  /*
  HTTP 요청 헤더를 저장하는 함수
  */
  
  char buf[MAXLINE]="";
  int header_index = 0;

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    // fprintf(stderr, "requesthdrs: %s\n", buf);
    request_headers[header_index] = strdup(buf);
    header_index++;

    if (header_index >= 100) {
      break;
    }

    Rio_readlineb(rp, buf, MAXLINE);
  }
}

void proxy_request(int connfd, int serverfd, char *hostname, char *port, char *filename, char **request_headers) {
  /*
  클라이언트로부터 받은 요청을 토대로, 서버에 요청을 보낸 뒤 반환값을 클라이언트에게 돌려주는 함수 
  */

  char buf[MAXLINE] = "";
  int content_length = 0, header_index = 0;
  size_t n;
  rio_t rio;

  // HTTP 요청 라인 & 요청 헤더 보내기
  Rio_readinitb(&rio, serverfd); // rio 버퍼 초기화
  sprintf(buf, "GET %s HTTP/1.0\r\n", filename); // HTTP 요청 라인
  
  while (request_headers[header_index] != NULL) {
    if (strstr(request_headers[header_index], "User-Agent") != NULL) {
      sprintf(buf + strlen(buf), "%s", user_agent_hdr);
    } else if (strstr(request_headers[header_index], "Proxy-Connection") != NULL) {
      sprintf(buf + strlen(buf), "Proxy-Connection: close\r\n");
    } else {
      sprintf(buf + strlen(buf), "%s\r\n", request_headers[header_index]);
    }

    header_index++;
  }

  Rio_writen(serverfd, buf, strlen(buf));

  // HTTP 응답 받는 부분
  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {    
    if (strstr(buf, "Content-length") != NULL) { // content-length만 따로 저장
      char *tmp = strchr(buf, ':');
      content_length = atoi(tmp+1);
    }

    Rio_writen(connfd, buf, strlen(buf));

    if (strcmp(buf, "\r\n") == 0) { // 응답 헤더까지 끝나면
      break;
    }
  } 

  char *content;
  content = (char *)malloc((size_t)content_length);
    
  Rio_readnb(&rio, content, (size_t)content_length);
  Rio_writen(connfd, content, (size_t)content_length);

  free(content);
  for (int i=0; request_headers[i] != NULL; i++) {
    free(request_headers[i]);
    request_headers[i] = NULL;
  }
  Close(serverfd);
}

void send_response_with_cache(int connfd, Cache_Node *cache_ptr, char **request_headers) {
  /*
  캐시 적중 시, 서버와 연결하지 않고 클라이언트에게 바로 응답 전송하는 함수
  */
  char buf[MAXLINE]="";

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
  sprintf(buf + strlen(buf), "Connection: close\r\n");
  sprintf(buf + strlen(buf), "Content-length: %u\r\n", cache_ptr->size);
  sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", cache_ptr->type);
  Rio_writen(connfd, buf, strlen(buf));

  Rio_writen(connfd, cache_ptr->content, cache_ptr->size);
}

/*
1. HTTP/1.0 GET 요청 받기
=> 요청 라인 파싱 (method URL version) => (method hostname filename version)
=> 절대주소 상대주소로 파싱 http://localhost:30946/home.html => /home.html
=> 호스트 이름과 포트 번호 파싱 (나중에 서버에 요청 보낼 때 사용)

=> 요청 헤더 파싱
=> Host: 호스트 이름으로 (그냥 냅둬도 될듯?)
=> User-Agent: 줬던 걸로
=> Connection: close 
=> Proxy-Connection: close
=> 다른 요청 헤더 있으면, 수정하지 말고 그대로 서버에 보내기

2. GET 요청한 URI에 따라서, 웹서버에 요청 보내기
3. 웹 서버의 응답 받아서, 그래도 클라이언트에게 전달하기
*/