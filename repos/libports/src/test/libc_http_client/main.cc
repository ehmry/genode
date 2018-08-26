#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static constexpr bool multithreaded = false;

static constexpr char const *server = "94.130.141.228"; /* genode.org */
static constexpr int port = 80;

static struct sockaddr_in addr;
static sockaddr const *paddr;

void test(int thread)
{
	for (int i = 1; i <= 100; i++) {

		printf("[%d] i: %d\n", thread, i);

		int s = socket(AF_INET, SOCK_STREAM, 0);

		printf("[%d] socket() returned %d\n", thread, s);

		int ret = connect(s, paddr, sizeof(addr));

		printf("[%d] connect() returned %d\n", thread, ret);

		if (ret != 0) {
			printf("[%d] Error: connect() failed, errno: %d\n", thread, errno);
			exit(-1);
		}

		char const *request = "GET / HTTP/1.0\r\n\r\n";

		ssize_t bytes_written = write(s, request, strlen(request));

		printf("[%d] write() returned %zd\n", thread, bytes_written);

		char response[64*1024] = { 0 };

		ssize_t bytes_read_total = 0;
		ssize_t bytes_read = 0;
		
		do {
			bytes_read = read(s, &response[bytes_read_total], sizeof(response) - bytes_read_total);
			printf("[%d] read() returned %zd\n", thread, bytes_read);
			bytes_read_total += bytes_read;
		} while (bytes_read > 0);

		printf("[%d] bytes read: %zd\n", thread, bytes_read_total);

		if (bytes_read_total != 10975) {
			printf("[%d] Error: read size mismatch\n", thread);
			exit(-1);
		}

		//printf("[%d] response:\n%s\n", thread, response);

		close(s);
	}
}


void *thread_func(void *arg)
{
	test(*(int*)arg);
	return 0;
}


int main()
{
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(server);

	paddr = reinterpret_cast<sockaddr const *>(&addr);

	if (multithreaded) {

		pthread_t t1;
		int arg1 = 1;
		pthread_create(&t1, 0, thread_func, &arg1);

		pthread_t t2;
		int arg2 = 2;
		pthread_create(&t2, 0, thread_func, &arg2);

		pthread_join(t1, 0);
		pthread_join(t2, 0);

	} else {
		test(0);
	}

	return 0;
}
