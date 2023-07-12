#include <stdio.h>

#include "vector.c"

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"

int main(int argc, char const *argv[])
{
	Vec *vec = v_create(sizeof(int));
	printf("Created vector\n");

	int test = 17;
	v_push(vec, &test);
	printf("Pushed %d\n", test);
	test = 42;
	v_push(vec, &test);
	printf("Pushed %d\n", test);

	VecIter *iter = v_iter(vec);
	printf("Created iterator\n");

	int tmp;
	do
	{
		tmp = vi_next_int(iter);
		printf("%d\n", tmp);
	}
	while (tmp != NULL);

	return 0;
}

#pragma GCC pop_options
