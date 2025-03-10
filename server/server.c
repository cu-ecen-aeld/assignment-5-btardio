#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define BUFFER_SIZE 999999

// http://gnu.cs.utah.edu/Manuals/glibc-2.2.3/html_chapter/libc_16.html

// NOTE: using AF_INET is not bidirectional
struct sockaddr_in sockaddrs[FD_SETSIZE];

void sig_handler(int signo)
{
  if (signo == SIGINT) {
    printf("Caught signal, exiting");
    exit(0);
  }
  if (signo == SIGTERM) {
    printf("Caught signal, exiting");
    exit(0);
  }
}




void log_and_print(int priority, char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("Errno: %d\n", errno);
    perror("Error:");
    fflush(stdout);

    va_start(args, fmt);
    vsyslog(priority, fmt, args);
    va_end(args);
}



int
read_from_client (int filedes)
{
  char fbuffer[BUFFER_SIZE];
  char buffer[BUFFER_SIZE];
  //char writebuffer[512];
  int nbytes;
  int sbytes;

  FILE *file_pointer;


  nbytes = read (filedes, buffer, BUFFER_SIZE);

  if (nbytes < 0)
    {
      /* Read error. */
      perror ("read");
      exit (EXIT_FAILURE);
    }
  else if (nbytes == 0)
    /* End-of-file. */
    return -1;
  else
    {
      

      file_pointer = fopen("/var/tmp/aesdsocketdata", "a");

      if ( file_pointer == NULL ){
          log_and_print(LOG_ERR, "Error writing to file.\n", NULL);
          return -1;
      }

      if (fputs(buffer, file_pointer) == EOF) {
          perror("Error writing to file");
          fclose(file_pointer);
          return -1;
      }

      if (fclose(file_pointer) == EOF) {
          perror("Error closing the file");
          return -11;
      }
      
      FILE* file = fopen("/var/tmp/aesdsocketdata", "r");

      if (file == NULL) {
        perror("Error opening file");
        return 1;
      }

      fseek(file, 0, SEEK_END);
      long file_size = ftell(file);
      fseek(file, 0, SEEK_SET);

      size_t bytes_read = fread(fbuffer, 1, file_size, file);
      if (bytes_read != (size_t)file_size) {
        perror("Error reading file");
        fclose(file);
        return 1;
      }

      fbuffer[file_size] = '\0';

      fclose(file);

      sbytes = write(filedes, fbuffer, file_size);
      

      if (sbytes < 0){
      
          perror("write");
          exit(EXIT_FAILURE);
      }

      return 0;
    }
}




int
make_socket (uint16_t port)
{
  int sock;
  struct sockaddr_in name;

  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      perror ("socket");
      exit (EXIT_FAILURE);
    }

  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      perror ("bind");
      exit (EXIT_FAILURE);
    }

  return sock;
}



void *safe_malloc(size_t n)
{
    void *p = malloc(n);
    if (p == NULL) {
        log_and_print(LOG_ERR, "Fatal: failed to allocate %zu bytes.\n", n);
        abort();
    }
    return p;
}


int pmain(void) {

    

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        log_and_print(LOG_ERR, "Unable to create signal handler.\n", NULL);
    }


    if (signal(SIGTERM, sig_handler) == SIG_ERR) {
        log_and_print(LOG_ERR, "Unable to create signal handler.\n", NULL);
    }


    size_t s_size;

    struct in_addr my_s_addr;

    fd_set active_fd_set, read_fd_set;

    inet_pton(AF_INET, "127.0.0.1", &my_s_addr);

    int s_fd = make_socket(9000);

    if (s_fd < 0) {
        log_and_print(LOG_ERR, "Unable to create socket.\n", NULL);
        return -1;
    }

    
    int l_rval = listen(s_fd, 3);


    if ( l_rval < 0 ) {
        log_and_print(LOG_ERR, "Unable to listen on port.\n", NULL);
    }

    struct sockaddr_in addr_connector;

    s_size = sizeof (addr_connector);


  /* Initialize the set of active sockets. */
  FD_ZERO (&active_fd_set);
  FD_SET (s_fd, &active_fd_set);

  while (1)
    {
      /* Block until input arrives on one or more active sockets. */
      read_fd_set = active_fd_set;
      if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
          perror ("select");
          exit (EXIT_FAILURE);
        }


      /* Service all the sockets with input pending. */
      for (int i = 0; i < FD_SETSIZE; ++i)
        if (FD_ISSET (i, &read_fd_set))
          {
            if (i == s_fd)
              {
                /* Connection request on original socket. */
                int new;
                s_size = sizeof (addr_connector);

                new = accept(s_fd, (struct sockaddr*) &addr_connector, (unsigned int *) &s_size); //(struct sockaddr *) &addr_connector, NULL);
                if ( new < 0 ) {
                    log_and_print(LOG_ERR, "Unable to accept.\n", NULL);
                    
                } else {
                    sockaddrs[new] = addr_connector;
                }
                FD_SET (new, &active_fd_set);
              }
              else
              {
                /* Data arriving on an already-connected socket. */
                if (read_from_client (i) < 0)
                  {
                    close (i);
                    FD_CLR (i, &active_fd_set);
                  }
              }
          }
    }
}

int main(void){

    if (remove("/var/tmp/aesdsocketdata") == 0) {
    } else {
        perror("Error deleting file");
    }

    pid_t p = fork();

    if ( p == 0 ) {
        pmain();
    }
    else {

    }

}
