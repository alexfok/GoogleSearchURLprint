// module: alex_test.c
// runs Google search and prints out first URL and Title found
// author: Alex Fok
// modules: 
//	cJSON.c - JSON parser
//	alext_test.c - main program
// compilation: gcc cJSON.c alext_test.c -o alext_test
// usage: alex_test "video%20chat"
// sample search URL: "ajax/services/search/web?v=1.0&q=Earth%20Day"
 
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include "cJSON.h"

void parseJSON(char *text);
int create_tcp_socket();
char *get_ip(char *host);
char *build_get_query(char *host, char *page);
void usage();
 
#define HOST "ajax.googleapis.com"
#define SEARCH_URL "ajax/services/search/web?v=1.0&q="
#define PORT 80
#define USERAGENT "ALEXGET 1.0"
#define RES_MAX_SIZE 4096
#define URL_MAX_SIZE 1024

int main(int argc, char **argv)
{
  struct sockaddr_in *remote;
  int sock;
  int tmpres;
  char *ip;
  char *get;
  char buf[BUFSIZ+1];
  char *host;
  char page[URL_MAX_SIZE];
  char json_res[RES_MAX_SIZE];

  if(argc != 2){
    usage();
    exit(2);
  }  

  host = HOST;
  if(argc == 2){
    snprintf(page, URL_MAX_SIZE,"%s%s", SEARCH_URL,argv[1]);
  }

  sock = create_tcp_socket();
  ip = get_ip(host);
  fprintf(stderr, "IP is %s\n", ip);
  remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in *));
  remote->sin_family = AF_INET;
  tmpres = inet_pton(AF_INET, ip, (void *)(&(remote->sin_addr.s_addr)));
  if( tmpres < 0)  
  {
    perror("Can't set remote->sin_addr.s_addr");
    exit(1);
  }else if(tmpres == 0)
  {
    fprintf(stderr, "%s is not a valid IP address\n", ip);
    exit(1);
  }
  remote->sin_port = htons(PORT);
 
  if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0){
    perror("Could not connect");
    exit(1);
  }
  get = build_get_query(host, page);
  fprintf(stderr, "Query is:\n<<START>>\n%s<<END>>\n", get);
 
  //Send the query to the server
  int sent = 0;
  while(sent < strlen(get))
  {
    tmpres = send(sock, get+sent, strlen(get)-sent, 0);
    if(tmpres == -1){
      perror("Can't send query");
      exit(1);
    }
    sent += tmpres;
  }
  //now it is time to receive the page
  memset(buf, 0, sizeof(buf));
  // Buffer for page chunks aggregation
  memset(json_res, 0, sizeof(json_res));

  int htmlstart = 0;
  char * htmlcontent;
  while((tmpres = recv(sock, buf, BUFSIZ, 0)) > 0){
    if(htmlstart == 0)
    {
      /* Under certain conditions this will not work.
      * If the \r\n\r\n part is splitted into two messages
      * it will fail to detect the beginning of HTML content
      */
      htmlcontent = strstr(buf, "\r\n\r\n");
      if(htmlcontent != NULL){
        htmlstart = 1;
        htmlcontent += 4;

      }
    }else{
      htmlcontent = buf;
    }
    if(htmlstart){
      /* fprintf(stdout, "%s", htmlcontent);*/
//     fprintf(stdout,"\n\njson2: %s, \n\nhtml2: %s\n\n",json_res, htmlcontent);
      strncat(json_res, htmlcontent, RES_MAX_SIZE);
	
    }
 
    memset(buf, 0, tmpres);
  }

//   fprintf(stdout, "%s", json_res);
   /* Parse JSON result */
   parseJSON(json_res);

  if(tmpres < 0)
  {
    perror("Error receiving data");
    return -1;
  }

  free(get);
  free(remote);
  free(ip);
  close(sock);
  return 0;
}

/* Parse text to JSON, then render back to text, and print! */
void parseJSON(char *text)
{
        char *out;cJSON *json;

        json=cJSON_Parse(text);
        if (!json) {printf("Error before: [%s]\n",cJSON_GetErrorPtr());}
        else
        {

        cJSON *responseData = cJSON_GetObjectItem(json,"responseData");
        cJSON *results = cJSON_GetObjectItem(responseData,"results");

	cJSON *firstRes = results->child;
	char* str_value = cJSON_GetObjectItem(firstRes,"unescapedUrl")->valuestring;
	char* str_value1 = cJSON_GetObjectItem(firstRes,"titleNoFormatting")->valuestring;

                out=cJSON_Print(firstRes);
		fprintf(stdout,"First URL: %s\n", str_value);
		fprintf(stdout,"First Title: %s\n", str_value1);
                
//		out=cJSON_Print(json);
                cJSON_Delete(json);
//                printf("%s\n",out);
                free(out);
        }
}


void usage()
{
  fprintf(stderr, "USAGE: alex_test search_term\n\
\tsearch_term: the search term. ex: video, default: video\n");
}
 
 
int create_tcp_socket()
{
  int sock;
  if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
    perror("Can't create TCP socket");
    exit(1);
  }
  return sock;
}
 
 
char *get_ip(char *host)
{
  struct hostent *hent;
  int iplen = 15; //XXX.XXX.XXX.XXX
  char *ip = (char *)malloc(iplen+1);
  memset(ip, 0, iplen+1);
  if((hent = gethostbyname(host)) == NULL)
  {
    herror("Can't get IP");
    exit(1);
  }
  if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL)
  {
    perror("Can't resolve host");
    exit(1);
  }
  return ip;
}
 
char *build_get_query(char *host, char *page)
{
  char *query;
  char *getpage = page;
  char *tpl = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\n\r\n";
  if(getpage[0] == '/'){
    getpage = getpage + 1;
    fprintf(stderr,"Removing leading \"/\", converting %s to %s\n", page, getpage);
  }
  // -5 is to consider the %s %s %s in tpl and the ending \0
  query = (char *)malloc(strlen(host)+strlen(getpage)+strlen(USERAGENT)+strlen(tpl)-5);
  sprintf(query, tpl, getpage, host, USERAGENT);
  return query;
}

