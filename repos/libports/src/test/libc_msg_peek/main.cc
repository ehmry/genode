
#include <libc/component.h>
#include <util/string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

void server()
{
    struct sockaddr_in server, client;
    socklen_t clilen = sizeof(client);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int connection;

    Genode::memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(80);

    bind(sock, (struct sockaddr *)&server, sizeof(server));
    listen(sock, 1);

 	Genode::log("accept");
   connection = accept(sock, (struct sockaddr *)&client, &clilen);
   if (connection < 0) {
      perror("accept failed");
      exit(connection);
   }
	Genode::log("accepted");

    char sbuf[2] = {};
    while(recv(connection, sbuf, 1, MSG_PEEK) > 0){
        Genode::log("peek: ", Genode::Cstring(sbuf));
        recv(connection, sbuf, 1, 0);
    }

    close(connection);
    close(sock);
}

void Libc::Component::construct(Libc::Env &)
{
    Genode::log("MSG_PEEK test");
    Libc::with_libc([&] () {
            server();
            });
}
