/*
 * The BYTE Sieve of Eratosthenes benchmark.
 *
 * After Jim and Gary Gilbreath, "A High-Level Language Benchmark", BYTE,
 * September 1981, and "Eratosthenes Revisited", BYTE, January 1983,
 * which compared dozens of languages and machines on this program: count
 * the primes below 16,384 using a flag array of 8,191 odd numbers,
 * repeated ITERS times. Each pass finds 1,899 primes.
 */
#include <stdio.h>

#ifndef ITERS
#define ITERS 10
#endif

#define TRUE  1
#define FALSE 0
#define SIZE  8190

char flags[SIZE + 1];

int main(void)
{
    int i, prime, k, count = 0, iter;
    printf("%d iterations\n", ITERS);
    for (iter = 1; iter <= ITERS; iter++) {
        count = 0;
        for (i = 0; i <= SIZE; i++)
            flags[i] = TRUE;
        for (i = 0; i <= SIZE; i++) {
            if (flags[i]) {
                prime = i + i + 3;
                k = i + prime;
                while (k <= SIZE) {
                    flags[k] = FALSE;
                    k += prime;
                }
                count = count + 1;
            }
        }
    }
    printf("%d primes\n", count);
    return 0;
}
