#include <stdio.h>

#include "vector.h"
#include "vector.c"

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"

static void vdebug_print_info(Vec *vec)
{
	printf("len: %zu | cap: %zu | offset: %zu | first: %p | last: %p\n",
		vec->len,
		vec->cap,
		vec->offset,
		vec->first,
		vec->last);
}

static void vdebug_print_raw_mem(Vec *vec)
{
	for (size_t i = 0; i < vec->len * vec->elem_size; i++)
	{
		printf("%02X ", ((char *)vec->data)[i]);

		if (((i + 1) & (vec->elem_size - 1)) == 0)
		{
			printf(" ");
		}
	}
	printf("\n");
}


int main(int argc, char const *argv[])
{
	Vec *vec = v_create(sizeof(int));
	printf("Created vector\n");

	int test = 0x17;
	v_push(vec, &test);
	printf("Pushed %x\n", test);
	test = 0x42;
	v_push(vec, &test);
	printf("Pushed %x\n", test);
	test = 0x360;
	v_push(vec, &test);
	printf("Pushed %x\n", test);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	int last;
	v_last(vec, &last);
	printf("Last is %x\n", last);

	int popped;
	v_pop(vec, &popped);
	printf("Popped %x\n", popped);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	VecIter *iter = v_iter(vec);
	printf("Created iterator\n");

	int tmp;
	while (vi_next(iter, &tmp) == 0)
	{
		printf("%d\n", tmp);
	}

	return 0;
}

#pragma GCC pop_options
