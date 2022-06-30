#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUF 256

void Usage(char *progname);

int LookupName(char *name,
               unsigned short port,
               struct sockaddr_storage *ret_addr,
               size_t *ret_addrlen);

int Connect(const struct sockaddr_storage *addr,
            const size_t addrlen,
            int *ret_fd);

int main(int argc, char **argv)
{
  if (argc != 3)
  {
    Usage(argv[0]);
  }

  unsigned short port = 0;
  if (sscanf(argv[2], "%hu", &port) != 1)
  {
    Usage(argv[0]);
  }

  // Get an appropriate sockaddr structure.
  struct sockaddr_storage addr;
  size_t addrlen;
  if (!LookupName(argv[1], port, &addr, &addrlen))
  {
    Usage(argv[0]);
  }

  // Connect to the remote host.
  int socket_fd;
  if (!Connect(&addr, addrlen, &socket_fd))
  {
    Usage(argv[0]);
  }
  
  struct addrinfo hints, *results;
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  char address[15];
  int hold = getaddrinfo(argv[1], NULL, &hints, &results);
  
  if (results->ai_family == AF_INET)
  {
    struct sockaddr_in *v4addr = (struct sockaddr_in *)(results->ai_addr);
    v4addr->sin_port = htons(port);
    inet_ntop(AF_INET,&(v4addr->sin_addr),address,INET_ADDRSTRLEN);
  }
  else if (results->ai_family == AF_INET6)
  {
    struct sockaddr_in6 *v6addr = (struct sockaddr_in6 *)(results->ai_addr);
    v6addr->sin6_port = htons(port);
    inet_ntop(AF_INET6,&(v6addr->sin6_addr),address,INET6_ADDRSTRLEN);
  }
  
  printf("established connection with server %s \n", address);
  
  char readbuf[BUF] = "";
  while (1)
  {
    read(0,readbuf, BUF -1);
    if(strcmp(readbuf,"exit\n\0") == 0) break;
    
    int wres = write(socket_fd, readbuf, BUF -1);
    if (wres == 0)
    {
      printf("socket closed prematurely \n");
      close(socket_fd);
      return EXIT_FAILURE;
    }
    if (wres == -1)
    {
      if (errno == EINTR)
        continue;
      printf("socket write failure \n");
      close(socket_fd);
      return EXIT_FAILURE;
    }
    
    memset(readbuf, 0, BUF);
    int res = read(socket_fd, readbuf, BUF - 1);
    printf("%s\n",readbuf);
    if (res == 0)
    {
      printf("socket closed prematurely \n");
      close(socket_fd);
      return EXIT_FAILURE;
    }
    if (res == -1)
    {
      if (errno == EINTR)
        continue;
      printf("socket read failure \n");
      close(socket_fd);
      return EXIT_FAILURE;
    }
    readbuf[res] = '\0';

  }

  // Clean up.
  close(socket_fd);
  return EXIT_SUCCESS;
}

void Usage(char *progname)
{
  printf("usage: %s  hostname port \n", progname);
  exit(EXIT_FAILURE);
}

int LookupName(char *name,
               unsigned short port,
               struct sockaddr_storage *ret_addr,
               size_t *ret_addrlen)
{
  struct addrinfo hints, *results;
  int retval;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  // Do the lookup by invoking getaddrinfo().
  if ((retval = getaddrinfo(name, NULL, &hints, &results)) != 0)
  {
    printf("getaddrinfo failed: %s", gai_strerror(retval));
    return 0;
  }

  // Set the port in the first result.
  if (results->ai_family == AF_INET)
  {
    struct sockaddr_in *v4addr =
        (struct sockaddr_in *)(results->ai_addr);
    v4addr->sin_port = htons(port);
  }
  else if (results->ai_family == AF_INET6)
  {
    struct sockaddr_in6 *v6addr =
        (struct sockaddr_in6 *)(results->ai_addr);
    v6addr->sin6_port = htons(port);
  }
  else
  {
    printf("getaddrinfo failed to provide an IPv4 or IPv6 address \n");
    freeaddrinfo(results);
    return 0;
  }

  // Return the first result.
  assert(results != NULL);
  memcpy(ret_addr, results->ai_addr, results->ai_addrlen);
  *ret_addrlen = results->ai_addrlen;

  // Clean up.
  freeaddrinfo(results);
  return 1;
}

int Connect(const struct sockaddr_storage *addr,
            const size_t addrlen,
            int *ret_fd)
{
  // Create the socket.
  int socket_fd = socket(addr->ss_family, SOCK_STREAM, 0);
  if (socket_fd == -1)
  {
    printf("socket() failed: %s", strerror(errno));
    return 0;
  }

  // Connect the socket to the remote host.
  int res = connect(socket_fd,
                    (const struct sockaddr *)(addr),
                    addrlen);
  if (res == -1)
  {
    printf("connect() failed: %s", strerror(errno));
    return 0;
  }

  *ret_fd = socket_fd;
  return 1;
}