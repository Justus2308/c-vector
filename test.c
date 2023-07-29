#include <stdio.h>

#include "vector.h"
#include "vector.c"


static void vdebug_print_info(Vec *vec)
{
	printf("len: %zu | cap: %zu | offset: %zu | real cap: %zu | first: %p | last: %p\n",
		vec->len,
		vec->cap,
		vec->offset,
		vinternal_real_cap(vec),
		vec->first,
		vec->last);
}

static void vdebug_print_raw_mem(Vec *vec)
{
	if (vec->len == 0)
	{
		printf("<EMPTY>\n\n");
		return;
	}

	for (size_t i = 0; i < vec->len * vec->elem_size; i++)
	{
		printf("%02X ", ((char *)vec->data)[i]);

		if (((i + 1) & (vec->elem_size - 1)) == 0)
		{
			printf(" ");
		}
	}
	printf("\n\n");
}


int main(void)
{
	size_t i;

	v_perror("test error", VE_NOCAP);

	Vec *vec = v_create(sizeof(int));
	printf("Created vector\n");

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	int test = 0x17;
	v_push(vec, &test);
	printf("Pushed : 0x%x\n", test);
	test = 0x42;
	v_push(vec, &test);
	printf("Pushed : 0x%x\n", test);
	test = 0x360;
	v_push(vec, &test);
	printf("Pushed : 0x%x\n", test);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	int last;
	v_last(vec, &last);
	printf("Last is : 0x%x\n", last);

	int popped;
	v_pop(vec, &popped);
	printf("Popped : 0x%x\n", popped);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	int insert = 0x278;
	v_insert(vec, &insert, 1);
	printf("Inserted at [1] : 0x%x\n", insert);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	insert = 0x123;
	v_insert(vec, &insert, 5);
	printf("Inserted OOB at [5] : 0x%x\n", insert);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	int removed;
	v_remove(vec, &removed, 2);
	printf("Removed at [2] : 0x%x\n", removed);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	v_last(vec, &last);
	printf("Last is : 0x%x\n", last);

	v_remove(vec, &removed, 10);
	printf("Removed OOB at [10] : 0x%x\n", removed);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	test = 0x234;
	v_push(vec, &test);
	printf("Pushed : 0x%x\n", test);
	test = 0x420;
	v_push(vec, &test);
	printf("Pushed : 0x%x\n", test);
	test = 0x31;
	v_push(vec, &test);
	printf("Pushed : 0x%x\n", test);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	insert = 0x57;
	v_swap_insert(vec, &insert, 3);
	printf("Swap inserted at [3] : 0x%x\n", insert);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	v_swap_remove(vec, &removed, 2);
	printf("Swap removed at [2] : 0x%x\n", removed);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	void *raw = v_raw(vec);
	printf("Raw data from v_raw:\n");
	for (i = 0; i < vec->len * vec->elem_size; i++)
	{
		printf("%02X ", ((char *)raw)[i]);

		if (((i + 1) & (vec->elem_size - 1)) == 0)
		{
			printf(" ");
		}
	}
	printf("\n");

	void *raw_slice = v_raw_slice(vec, 1, 4);
	printf("Raw data from v_raw_slice[1...4]:\n");
	for (i = 0; i < 3 * vec->elem_size; i++)
	{
		printf("%02X ", ((char *)raw_slice)[i]);

		if (((i + 1) & (vec->elem_size - 1)) == 0)
		{
			printf(" ");
		}
	}
	printf("\n");

	Vec *slice = v_slice(vec, 1, 4);
	printf("Raw data from v_slice[1...4]:\n");
	vdebug_print_info(slice);
	vdebug_print_raw_mem(slice);

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	int prepended[] = {0x123, 0x456};
	v_prepend(vec, &prepended, 2);
	printf("Prepended :");
	for (i = 0; i < 2; i++)
	{
		printf(" 0x%x", prepended[i]);
	}
	printf("\n");

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	int appended[] = {0x1, 0x2, 0x3, 0x4};
	v_append(vec, &appended, 4);
	printf("Appended :");
	for (i = 0; i < 4; i++)
	{
		printf(" 0x%x", appended[i]);
	}
	printf("\n");

	vdebug_print_info(vec);
	vdebug_print_raw_mem(vec);

	v_last(vec, &last);
	printf("Last is : 0x%x\n", last);



	VecIter *iter = v_iter(vec);
	printf("Created iterator\n");

	int tmp;
	while (vi_next(iter, &tmp) == 0)
	{
		printf("%d\n", tmp);
	}

	return 0;
}
