
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

int
main(int argc, char **argv)
{
   int val;
   int s;
   int cc;
   socklen_t l;

   l = sizeof(int);
   s = socket(AF_INET, SOCK_STREAM, 0);
   cc  = getsockopt(s, SOL_SOCKET, SO_RCVBUF,
                    &val, &l);
   if (cc < 0) {
     perror("getsockopt");
     close(s);
     return -1;
   }
   printf("SO_RCVBUF: %d\n" ,val);
   l = sizeof(int);
   cc  = getsockopt(s, SOL_SOCKET, SO_SNDBUF,
                    &val, &l);
   if (cc < 0) {
     perror("getsockopt");
     close(s);
     return -1;
   }

   printf("SO_SNDBUF %d\n" ,val);

   close(s);

   return 0;
}
