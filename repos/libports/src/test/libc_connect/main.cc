#include <base/log.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
	int s = socket(AF_INET, SOCK_STREAM, 0);

	Genode::log("socket() returned ", s);

	char const *server = "94.130.141.228"; /* genode.org */
	int port = 81;

	sockaddr_in const addr { 0, AF_INET, htons(port), { inet_addr(server) } };

	sockaddr const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	int ret = connect(s, paddr, sizeof(addr));

	Genode::log("connect() returned ", ret);

	return 0;
}
