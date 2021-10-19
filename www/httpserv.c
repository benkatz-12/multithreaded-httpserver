#include <stdio.h> /* basic IO */
#include <stdlib.h> /* exit */
#include <sys/socket.h> /* sockets */
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h> /* memset */
#include <strings.h> /* bzero, bcopy */
#include <unistd.h> /* close */
#include <pthread.h>

#define LISTENQ 10 /* Maximim number of client connections */
#define MAXBUF 65535 /* Maximum buffer size */

int open_servfd(int serv_port); //initalize server port to accept connections
void * thread(void* vargp); //Thread routine
char* ptopath(char* i_path); //converts input path to a fread-readable path
char* build_header(int f_size, char* extension, char* verson); //build http header

int main(int argc, char** argv){
    int serv_port, servfd, *clientfdp, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;
    

    if(argc != 2){
        fprintf(stderr, "Correct Usage: ./httpserv <port number>\n");
        exit(1);
    }

    serv_port = atoi(argv[1]);
    
    if((servfd = open_servfd(serv_port)) < 0){
        exit(1);
    }

    while(1){
        clientfdp = malloc(sizeof(int)); /* malloc client socket descriptor to be able to free later // clientaddr struct is not used, so overwrite is OK*/
        if((*clientfdp = accept(servfd, (struct sockaddr *)&clientaddr, &clientlen)) < 0){
            perror("Accept");
            exit(1);
        }
        pthread_create(&tid, NULL, thread, clientfdp);
    }

    close(servfd);
}


/*
 * build_header - builds header with message information
 */
char* build_header(int f_size, char* extension, char* version){ // ADD HTTP VERSION AS WELL
    //get number of digits in f_size
    int len = 0;
    char* type;
    int fn_size = f_size;
    do{
        f_size /= 10;
        len++;
    }while(f_size != 0);
    //change extension to http content-type
    if(strcmp(extension, ".css") == 0){
        type = "text/css";
    }else if(strcmp(extension, ".js") == 0){
        type = "application/javascript\r\n";
    }else if(strcmp(extension, ".jpg") == 0){
        type = "image/jpg";
    }else if(strcmp(extension, ".gif") == 0){
        type = "image/gif";
    }else if(strcmp(extension, ".png") == 0){
        type = "image/png";
    }else if(strcmp(extension, ".txt") == 0){
        type = "text/plain";
    }else if(strcmp(extension, ".html") == 0){
        type = "text/html";
    }

    char header[len + 22 + 49 + 3 + 8]; //22 is the longest extension possible and header already verified | 49 is the template | 3 is the version | 8 is <CLRF>
    char* r_header = (char*)malloc(len + 22 + 49 + 3 + 8);
    snprintf(header, sizeof(header), "HTTP/%s 200 OK\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", version, fn_size, type);
    memcpy(r_header, header, strlen(header));
    
    return r_header;
}

/*
 * get - retreives requested THING and sends it along the connection
 */
int get(char* buf, char* path, int clientfd, char* version){
    FILE* fd;
    int c, f_size;
    char* header;
    char* extension;
    fd = fopen(path,"rb");
    
    if(fd){
        fseek(fd, 0, SEEK_END);
        f_size = ftell(fd);
        if(strstr(path, "fancybox") != NULL){
            extension = ".js";
        }else{
            extension = strchr(path, '.');
        }
        header = build_header(f_size, extension, version);
        rewind(fd);
        
        c = snprintf(buf, MAXBUF, "%s", header);
        if(f_size <= MAXBUF){
            c += fread(buf+c, 1, f_size, fd);
        }else{
            printf("big shit\n");  // HANDLE BIG FILES WITH LOOP OF SHIT
        }
        
        write(clientfd, buf, c);
        
        bzero(buf, MAXBUF);
        
    }else{
        perror("fopen");
        return -1;
    }
    //free(header);
    
    return 0;
}


/*
 * ptopath - converts input path to a fread-readable path
 */
char* ptopath(char* i_path){
    if(strcmp(i_path, "/") == 0){
        return "www/index.html";
    }
    char* r_path = (char*)malloc(sizeof(char)*(strlen(i_path)+sizeof("www")));
    char prefix[3] = "www";
    memcpy(r_path, prefix, strlen(prefix));
    memcpy(r_path+3, i_path, strlen(i_path));
    return r_path;
}


/*
 * parse - parse HTTP request buffered at clientfd
 *       return -1 on failure
 */
int parse(int clientfd){
    
    int n;
    char buf[MAXBUF];
    const char* delim = "\r\n";
    char* request_line;
    char* method; 
    char* path;
    char* version;
    char* vers = (char*)malloc(3);
    
    
    while((n = read(clientfd, buf, MAXBUF)) != 0){
        if(n < 0){
            perror("Read");
            return -1;
        }
        printf("Server received the following request: %d bytes -  from %d \n%s\n",n,clientfd,buf);
        request_line = strtok(buf, delim);
        method = strtok(request_line, " ");
        path = strtok(NULL, " ");
        version = strtok(NULL, " ");
        memcpy(vers, strchr(version, '/')+1, 3);
        
        path = ptopath(path);
        printf("    method: %ld bytes - %s\n", strlen(method),method);
        printf("    path: %ld bytes - %s\n", strlen(path),path);
        printf("    version: %ld bytes - %s\n", strlen(version),version);
        
        bzero(buf, MAXBUF);
        
        if(strcmp(method, "GET")){
            
            get(buf, path, clientfd, vers);
            
        }else if (strcmp(method, "POST")){
            printf("POST\n");
        }
        
    }
    //free(path);
    
    free(vers);
    
    return 0;
}


/* Thread routine */
void * thread(void* vargp){
    int clientfd = *((int*)vargp);
    pthread_detach(pthread_self()); /* Detach current thread so there is no pthread_join to wait on */
    free(vargp); /* Free clientfdp to all accepting new clients */
    parse(clientfd);
    close(clientfd);
    fflush(stdout);
    
    return NULL;
}



/*
 * open_servfd - create socket and initialize to accept connection on given port
 *      reutrn -1 on failure
 */
int open_servfd(int serv_port){
    int servfd, optval=1;
    struct sockaddr_in serveraddr;

    /* Create socket descriptor */
    if((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Creating socket");
        return -1;
    }
    
    /* Eliminates "Address already in use" error from bind. Important because we are reusing server address/port */
    if(setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0){
        perror("Setting socket options");
        return -1;
    }

    /* Assign local IP address to socket */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)serv_port); 
    if (bind(servfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0){
        perror("Bind");
        return -1;
    }

    /* Make socket ready to accept incoming connection requests */
    if(listen(servfd, LISTENQ) < 0){
        perror("Listen");
        return -1;
    }
    
    return servfd;
}