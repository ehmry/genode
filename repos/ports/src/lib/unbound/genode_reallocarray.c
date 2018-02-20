#include <stdlib.h>

void* reallocarrayunbound(void *ptr, size_t nmemb, size_t size)
{
	size_t newsize = nmemb*size;

	if (newsize > size)
		return realloc(ptr, newsize);
	return NULL;
}
