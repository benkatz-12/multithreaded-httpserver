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
char* build_header(int f_size, char* extension, char* verson, int post_len); //build http header

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
char* build_header(int f_size, char* extension, char* version, int post_len){ // ADD HTTP VERSION AS WELL
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
        type = "application/javascript";
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
    }else{
    	type = "NONE";
    }

    char header[len + 22 + 49 + 3 + 8]; //22 is the longest extension possible and header already verified | 49 is the template | 3 is the version | 8 is <CLRF>
    char* r_header = (char*)malloc(len + 22 + 49 + 3 + 8);
    snprintf(header, sizeof(header), "HTTP/%s 200 OK\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", version, fn_size + post_len, type);
    memcpy(r_header, header, strlen(header));
    
    return r_header;
}

/*
 * send - wrapper function to send TCP packet (HTML header and data)
 */
int send_packet(){
	return 0;
}


/*
 * get - retreives requested THING and sends it along the connection
 */
int get(char* buf, char* path, int clientfd, char* version, int post, char* post_data){
    FILE* fd;
    int c, f_size, post_len;
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

        if(post_data){
        	post_len = strlen(post_data) + 46;// 46 for httpheaders
        }else{post_len = 0;}
        header = build_header(f_size, extension, version, post_len); 
        rewind(fd);
        //write header
        c = snprintf(buf, strlen(header)+1, "%s", header); //strlen(header)+1 because strlen ends on \n;
        write(clientfd, buf, c);
        c = 0;
        //write data section
    	int quotient = f_size / MAXBUF;
    	int remainder = f_size % MAXBUF;
    	for(int i = 0; i < quotient; i++){
    		c = fread(buf, 1, MAXBUF, fd);
    		write(clientfd, buf, c);
    	}
    	c = fread(buf, 1, remainder, fd); 
    	write(clientfd, buf, c);
    	bzero(buf, MAXBUF);
    	if(post){
    		printf("Post Data: %s\n", post_data);
    		char post_string[46 + strlen(post_data)]; // 46 is combined length of html headers 
    		snprintf(post_string, sizeof(post_string)+1, "<html><body><pre><h1>%s</h1></pre></body></html>", post_data);
    		//printf("Post String: %s",post_string);
    		write(clientfd, post_string, strlen(post_string));
    	}
        bzero(buf, MAXBUF);
        
    }else{
        perror("fopen");
        printf("Path: %s\n", path);
        return -1;
    }
    //free(header);
    
    return 0;
}



/*
 * post - handles post requests by injecting POSTDATA into .html file and sending the resulting file
 */
int post(char* buf){
	return 1; //just concat buffer and post data
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
    char* post_data;
    char* post_data_raw;
    char* cmp_method = (char*)malloc(4);
    
    while((n = read(clientfd, buf, MAXBUF)) != 0){
        if(n < 0){
            perror("Read");
            return -1;
        }

        post_data_raw = strstr(buf, "\r\n\r\n");
        post_data_raw = post_data_raw + 4;
        post_data = (char*)malloc(strlen(post_data_raw));
        memcpy(post_data, post_data_raw, strlen(post_data_raw));
        //printf("post Data: %s\n", post_data);
        //printf("Server received the following request: %d bytes -  from %d \n%s\n",n,clientfd,buf);
        request_line = strtok(buf, delim);
        method = strtok(request_line, " ");
        path = strtok(NULL, " ");
        version = strtok(NULL, " ");
        
        memcpy(vers, strchr(version, '/')+1, 3);
        memcpy(cmp_method, method, strlen(method));
        
        path = ptopath(path);
        // printf("    method: %ld bytes - %s\n", strlen(method),method);
        // printf("    path: %ld bytes - %s\n", strlen(path),path);
        // printf("    version: %ld bytes - %s\n", strlen(version),version);
        
        bzero(buf, MAXBUF);
        if(strcmp(cmp_method, "GET") == 0){
            get(buf, path, clientfd, vers, 0, NULL);
        }else if (strcmp(cmp_method, "POST") == 0){
            get(buf, path, clientfd, vers, 1, post_data);
        }
        
    }
    //free(path);
    free(post_data);
    free(cmp_method);
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
