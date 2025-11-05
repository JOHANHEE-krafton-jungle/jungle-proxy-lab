/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"
#include <string.h>

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  fprintf(stderr, "DEBUG: adder starts\n");

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p + 1);
    n1 = atoi(strchr(arg1, '=') + 1); // key1=100&key2=1023 형태
    n2 = atoi(strchr(arg2, '=') + 1);
    // n1 = atoi(arg1); // 100&1023 형태
    // n2 = atoi(arg2);
    fprintf(stderr, "%s, %s, %d, %d\n", arg1, arg2, n1, n2);
  }

  /* Make the response body */
  struct stat sbuf; 
  char *filename = "./cgi-bin/adder.html";
  char res_div[13] = "result-space";

  stat(filename, &sbuf);
  int srcfd = Open(filename, O_RDWR, 0); // 해당 파일 열고 (읽기 쓰기 가능)
  char *srcp = Mmap(filename, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, srcfd, 0); // 파일 내용 복사한 뒤에 (srcp)
  char *resp = strstr(srcp, res_div);

  strncat(content, srcp, srcp - resp);
  sprintf(content + strlen(content), "%d + %d = %d\r\n", n1, n2, n1 + n2);
  strcat(content, strlen(res_div)+resp+2);

  close(srcfd); // 해당 파일 닫고
  Munmap(srcp, sbuf.st_size); // mmap 매핑 해제

  /* Generate the HTTP response */
  printf("Content-type: text/html\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("\r\n");
  printf("%s", content);
  fflush(stdout);

  fprintf(stderr, "DEBUG: adder ends\n");

  exit(0);
}
/* $end adder */
