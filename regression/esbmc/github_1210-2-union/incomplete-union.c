
#include <assert.h>
#include <stdlib.h>
#include <string.h>

union incomplete;

extern union incomplete JJ;

int main()
{
	int k = 42;
	memcpy(&JJ, &k, sizeof(k));
	int j;
	memcpy(&j, &JJ, sizeof(k));
	assert(j == 42);
}

// struct incomplete { short x, y; };
