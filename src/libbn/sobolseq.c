/* Copyright (c) 2007 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Generation of Sobol sequences in up to 1111 dimensions, based on the
   algorithms described in:
   P. Bratley and B. L. Fox, Algorithm 659, ACM Trans.
   Math. Soft. 14 (1), 88-100 (1988),
   as modified by:
   S. Joe and F. Y. Kuo, ACM Trans. Math. Soft 29 (1), 49-57 (2003).

   Note that the code below was written without even looking at the
   Fortran code from the TOMS paper, which is only semi-free (being
   under the restrictive ACM copyright terms).  Then I went to the
   Fortran code and took out the table of primitive polynomials and
   starting direction #'s ... since this is just a table of numbers
   generated by a deterministic algorithm, it is not copyrightable.
   (Obviously, the format of these tables then necessitated some
   slight modifications to the code.)

   For the test integral of Joe and Kuo (see the main() program
   below), I get exactly the same results for integrals up to 1111
   dimensions compared to the table of published numbers (to the 5
   published significant digits).

   This is not to say that the authors above should not be credited for
   their clear description of the algorithm (and their tabulation of
   the critical numbers).  Please cite them.  Just that I needed
   a free/open-source implementation. */

#include "common.h"
#include <stdlib.h>
#include <math.h>
#include "bu/malloc.h"
#include "bn/rand.h"
#include "soboldata.h"

/* Period parameters */
#define NL_N 624
#define NL_M 397
#define NL_MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define NL_UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define NL_LOWER_MASK 0x7fffffffUL /* least significant r bits */

typedef struct bn_soboldata_s {
    unsigned sdim; /* dimension of sequence being generated */
    uint32_t *mdata; /* array of length 32 * sdim */
    uint32_t *m[32]; /* more convenient pointers to mdata, of direction #s */
    uint32_t *x; /* previous x = x_n, array of length sdim */
    unsigned *b; /* position of fixed point in x[i] is after bit b[i] */
    uint32_t n; /* number of x's generated so far */
    uint32_t NL_mt[NL_N]; /* the array for the state vector  */
    int NL_mti; /* mti==N+1 means mt[N] is not initialized */
} soboldata;

/* initializes NLmt[N] with a seed */
static void nlopt_init_genrand(soboldata *sd, unsigned long s)
{
    sd->NL_mt[0]= s & 0xffffffffUL;
    for (sd->NL_mti=1; sd->NL_mti<NL_N; sd->NL_mti++) {
	sd->NL_mt[sd->NL_mti] = (1812433253UL * (sd->NL_mt[sd->NL_mti-1] ^ (sd->NL_mt[sd->NL_mti-1] >> 30)) + sd->NL_mti);
	/* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
	/* In the previous versions, MSBs of the seed affect   */
	/* only MSBs of the array mt[].                        */
	/* 2002/01/09 modified by Makoto Matsumoto             */
	sd->NL_mt[sd->NL_mti] &= 0xffffffffUL;
	/* for >32 bit machines */
    }
}

/* generates a random number on [0,0xffffffff]-interval */
static uint32_t nlopt_genrand_int32(soboldata *sd)
{
    uint32_t y;
    static uint32_t mag01[2]={0x0UL, NL_MATRIX_A};

    if (sd->NL_mti >= NL_N) { /* generate N words at one time */
	int kk;

	if (sd->NL_mti == NL_N+1)   /* if init_genrand() has not been called, */
	    nlopt_init_genrand(sd, 5489UL); /* a default initial seed is used */

	for (kk=0;kk<NL_N-NL_M;kk++) {
	    y = (sd->NL_mt[kk]&NL_UPPER_MASK)|(sd->NL_mt[kk+1]&NL_LOWER_MASK);
	    sd->NL_mt[kk] = sd->NL_mt[kk+NL_M] ^ (y >> 1) ^ mag01[y & 0x1UL];
	}
	for (;kk<NL_N-1;kk++) {
	    y = (sd->NL_mt[kk]&NL_UPPER_MASK)|(sd->NL_mt[kk+1]&NL_LOWER_MASK);
	    sd->NL_mt[kk] = sd->NL_mt[kk+(NL_M-NL_N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
	}
	y = (sd->NL_mt[NL_N-1]&NL_UPPER_MASK)|(sd->NL_mt[0]&NL_LOWER_MASK);
	sd->NL_mt[NL_N-1] = sd->NL_mt[NL_M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

	sd->NL_mti = 0;
    }

    y = sd->NL_mt[sd->NL_mti++];

    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}

/* generates a random number on [0,1) with 53-bit resolution*/
static double nlopt_genrand_res53(soboldata *sd)
{
    uint32_t a=nlopt_genrand_int32(sd)>>5, b=nlopt_genrand_int32(sd)>>6;
    return(a*67108864.0+b)*(1.0/9007199254740992.0);
}
/* These real versions are due to Isaku Wada, 2002/01/09 added */


/* generate uniform random number in [a,b) with 53-bit resolution,
 * added by SGJ */
double bn_sobol_urand(soboldata *sd, double a, double b)
{
    return(a + (b - a) * nlopt_genrand_res53(sd));
}

/* Return position (0, 1, ...) of rightmost (least-significant) zero bit in n.
 *
 * This code uses a 32-bit version of algorithm to find the rightmost
 * one bit in Knuth, _The Art of Computer Programming_, volume 4A
 * (draft fascicle), section 7.1.3, "Bitwise tricks and
 * techniques."
 *
 * Assumes n has a zero bit, i.e. n < 2^32 - 1.
 *
 */
static unsigned rightzero32(uint32_t n)
{
#if defined(__GNUC__) && \
    ((__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || __GNUC__ > 3)
    return __builtin_ctz(~n); /* gcc builtin for version >= 3.4 */
#else
    const uint32_t a = 0x05f66a47; /* magic number, found by brute force */
    static const unsigned decode[32] = {0,1,2,26,23,3,15,27,24,21,19,4,12,16,28,6,31,25,22,14,20,18,11,5,30,13,17,10,29,9,8,7};
    n = ~n; /* change to rightmost-one problem */
    n = a * (n & (-n)); /* store in n to make sure mult. is 32 bits */
    return decode[n >> 27];
#endif
}

/* generate the next term x_{n+1} in the Sobol sequence, as an array
   x[sdim] of numbers in (0,1).  Returns 1 on success, 0 on failure
   (if too many #'s generated) */
static int sobol_gen(soboldata *sd, double *x)
{
    unsigned c, b, i, sdim;

    if (sd->n == 4294967295U) return 0; /* n == 2^32 - 1 ... we would
					   need to switch to a 64-bit version
					   to generate more terms. */
    c = rightzero32(sd->n++);
    sdim = sd->sdim;
    for (i = 0; i < sdim; ++i) {
	b = sd->b[i];
	if (b >= c) {
	    sd->x[i] ^= sd->m[c][i] << (b - c);
	    x[i] = ((double) (sd->x[i])) / (1U << (b+1));
	}
	else {
	    sd->x[i] = (sd->x[i] << (c - b)) ^ sd->m[c][i];
	    sd->b[i] = c;
	    x[i] = ((double) (sd->x[i])) / (1U << (c+1));
	}
    }
    return 1;
}


static int sobol_init(soboldata *sd, unsigned sdim, unsigned long seed)
{
    unsigned i,j;

    if (!sdim || sdim > MAXDIM) return 0;

    sd->mdata = (uint32_t *) bu_malloc(sizeof(uint32_t) * (sdim * 32), "sobol mdata");
    if (!sd->mdata) return 0;

    /* mti==N+1 means mt[N] is not initialized */
    sd->NL_mti=NL_N+1;

    for (j = 0; j < 32; ++j) {
	sd->m[j] = sd->mdata + j * sdim;
	sd->m[j][0] = 1; /* special-case Sobol sequence */
    }
    for (i = 1; i < sdim; ++i) {
	uint32_t a = sobol_a[i-1];
	unsigned d = 0, k;

	while (a) {
	    ++d;
	    a >>= 1;
	}
	d--; /* d is now degree of poly */

	/* set initial values of m from table */
	for (j = 0; j < d; ++j)
	    sd->m[j][i] = sobol_minit[j][i-1];

	/* fill in remaining values using recurrence */
	for (j = d; j < 32; ++j) {
	    a = sobol_a[i-1];
	    sd->m[j][i] = sd->m[j - d][i];
	    for (k = 0; k < d; ++k) {
		sd->m[j][i] ^= ((a & 1) * sd->m[j-d+k][i]) << (d-k);
		a >>= 1;
	    }
	}
    }

    sd->x = (uint32_t *) bu_malloc(sizeof(uint32_t) * sdim, "sobol x");
    if (!sd->x) { bu_free(sd->mdata, "sobol x"); return 0; }

    sd->b = (unsigned *) bu_malloc(sizeof(unsigned) * sdim, "sobol b");
    if (!sd->b) { bu_free(sd->x, "sobol x"); bu_free(sd->mdata, "sobol mdata"); return 0; }

    for (i = 0; i < sdim; ++i) {
	sd->x[i] = 0;
	sd->b[i] = 0;
    }

    sd->n = 0;
    sd->sdim = sdim;

    if (seed) nlopt_init_genrand(sd, seed); 

    return 1;
}

static void sobol_destroy(soboldata *sd)
{
    bu_free(sd->mdata, "sobol mdata");
    bu_free(sd->x, "sobol x");
    bu_free(sd->b, "sobol b");
}

/************************************************************************/
/* API to Sobol sequence creation, which hides bn_soboldata structure
   behind an opaque pointer */

bn_soboldata bn_sobol_create(unsigned int sdim, unsigned long seed)
{
    bn_soboldata s = (bn_soboldata) bu_malloc(sizeof(soboldata), "sobol data");
    if (!s) return NULL;
    if (!sobol_init(s, sdim, seed)) { bu_free(s, "sobol data"); return NULL; }
    return s;
}

extern void bn_sobol_destroy(bn_soboldata s)
{
    if (s) {
	sobol_destroy(s);
	bu_free(s, "sobol");
    }
}

/* next vector x[sdim] in Sobol sequence, with each x[i] in (0,1) */
void bn_sobol_next01(bn_soboldata s, double *x)
{
    if (!sobol_gen(s, x)) {
	/* fall back on pseudo random numbers in the unlikely event
	   that we exceed 2^32-1 points */
	unsigned int i;
	for (i = 0; i < s->sdim; ++i)
	    x[i] = bn_sobol_urand(s, 0.0,1.0);
    }
}

/* next vector in Sobol sequence, scaled to (lb[i], ub[i]) interval */
void bn_sobol_next(bn_soboldata s, double *x,
	const double *lb, const double *ub)
{
    unsigned int i;
    bn_sobol_next01(s, x);
    for (i = 0; i < s->sdim; ++i) {
	x[i] = lb[i] + (ub[i] - lb[i]) * x[i];
    }
}

/* if we know in advance how many points (n) we want to compute, then
   adopt the suggestion of the Joe and Kuo paper, which in turn
   is taken from Acworth et al (1998), of skipping a number of
   points equal to the largest power of 2 smaller than n */
void bn_sobol_skip(bn_soboldata s, unsigned n, double *x)
{
    if (s) {
	unsigned int k = 1;
	while (k*2 < n) k *= 2;
	while (k-- > 0) sobol_gen(s, x);
    }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

