/*
*
*  Copyright (c) 2015 Intel Corporation. All rights reserved.
*  This software is available to you under the BSD license. For
*  license information, see the LICENSE file in the top level directory.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <shmem.h>
#include <shmemx.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>


/* return microseconds */
double perf_shmemx_wtime(void);

double perf_shmemx_wtime(void)
{
    double wtime = 0.0;

#ifdef CLOCK_MONOTONIC
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    wtime = tv.tv_sec * 1e6;
    wtime += (double)tv.tv_nsec / 1000.0;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    wtime = tv.tv_sec * 1e6;
    wtime += (double)tv.tv_usec;
#endif
    return wtime;
}

#ifdef CRAY_SHMEM
#define shmem_putmem_nbi(dest, source, nelems, pe) shmem_putmem_nb(dest, source, nelems, pe, NULL)
#define shmem_getmem_nbi(dest, source, nelems, pe) shmem_getmem_nb(dest, source, nelems, pe, NULL)
#endif

static char * aligned_buffer_alloc(int len)
{
    unsigned long alignment = 0;
    char *ptr1 = NULL, *ptr_aligned = NULL;
    size_t ptr_size = sizeof(uintptr_t);
    uintptr_t save_ptr1 = 0;

    alignment = getpagesize();

    ptr1 = shmem_malloc(ptr_size + alignment + len);
    assert(ptr1 != NULL);

    save_ptr1 = (uintptr_t)ptr1;

    /* reserve at least ptr_size before alignment chunk */
    ptr1 = (char *) (ptr1 + ptr_size);

    /* only offset ptr by alignment to ensure len is preserved */
    /* clear bottom bits to ensure alignment */
    ptr_aligned = (char *) ( ((uintptr_t) ((char *) (ptr1 + alignment)))
                                                & ~(alignment-1));

    /* embed org ptr address in reserved ptr_size space */
    memcpy((ptr_aligned - ptr_size), &save_ptr1, ptr_size);

    return ptr_aligned;
}

static void aligned_buffer_free(char * ptr_aligned)
{

    char * ptr_org;
    uintptr_t temp_p;
    size_t ptr_size = sizeof(uintptr_t);

    /* grab ptr */
    memcpy(&temp_p, (ptr_aligned - ptr_size), ptr_size);
    ptr_org = (char *) temp_p;

    shmem_free(ptr_org);
}

int static inline is_divisible_by_4(int num)
{
    assert(num >= 0);
    assert(sizeof(int) == 4);
    return (!(num & 0x00000003));
}

/*to be a power of 2 must only have 1 set bit*/
int static inline is_pow_of_2(unsigned int num)
{
    /*move first set bit all the way to right*/
    while(num && !((num >>=1 ) & 1));

    /*it will be 1 if its the only set bit*/
    return ((num == 1 || num == 0)? true : false);
}

void static init_array(char * const buf, int len, int my_pe_num)
{
    int i = 0;
    int array_size = len / sizeof(int);
    int * ibuf = (int *)buf;

    assert(is_divisible_by_4(len));

    for(i = 0; i < array_size; i++)
        ibuf[i] = my_pe_num;

}

void static inline validate_recv(char * buf, int len, int partner_pe)
{
    int i = 0;
    int array_size = len / sizeof(int);
    int * ibuf = (int *)buf;

    assert(is_divisible_by_4(len));

    for(i = 0; i < array_size; i++) {
        if(ibuf[i] != partner_pe)
            printf("validation error at index %d: %d != %d \n", i, ibuf[i],
                    partner_pe);
    }
}
