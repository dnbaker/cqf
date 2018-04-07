/*
 * ============================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017-02-04 03:40:58 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prashant Pandey (ppandey@cs.stonybrook.edu)
 *                  Rob Johnson (rob@cs.stonybrook.edu)
 *   Organization:  Stony Brook University
 *
 * ============================================================================
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <openssl/rand.h>

#include "include/gqf.h"
#include "include/hashutil.h"

int main(int argc, char **argv)
{
	QF qf;
	uint64_t qbits = atoi(argv[1]);
	uint64_t nhashbits = qbits + 8;
	uint64_t nslots = (1ULL << qbits);
	uint64_t nvals = 750*nslots/1000;
	uint64_t key_count = 1000;
	uint64_t *vals;

	/* Initialise the CQF */
	if (!qf_malloc(&qf, nslots, nhashbits, 0, LOCKS_FORBIDDEN, INVERTIBLE, 0)) {
		fprintf(stderr, "Can't allocate CQF.\n");
		abort();
	}

	qf_set_auto_resize(&qf);

	/* Generate random values */
	vals = (uint64_t*)malloc(nvals*sizeof(vals[0]));
	RAND_pseudo_bytes((unsigned char *)vals, sizeof(*vals) * nvals);
	for (uint64_t i = 0; i < nvals; i++) {
		vals[i] = (1 * vals[i]) % qf.metadata->range;
	}

	/* Insert vals in the CQF */
	for (uint64_t i = 0; i < nvals; i++) {
		if (!qf_insert(&qf, vals[i], 0, key_count)) {
			fprintf(stderr, "failed insertion for key: %lx %d.\n", vals[i], 50);
			abort();
		}
	}
	for (uint64_t i = 0; i < nvals; i++) {
		uint64_t count = qf_count_key_value(&qf, vals[i], 0);
		if (count < key_count) {
			fprintf(stderr, "failed lookup after insertion for %lx %ld.\n", vals[i],
							count);
			abort();
		}
	}

	char filename[] = "mycqf.cqf";
	fprintf(stdout, "Serializing the CQF to disk.\n");
	uint64_t total_size = qf_serialize(&qf, filename);
	if (total_size < sizeof(qfmetadata) + qf.metadata->total_size_in_bytes) {
		fprintf(stderr, "CQF serialization failed.\n");
		abort();
	}

	QF file_qf;
	fprintf(stdout, "Reading the CQF from disk.\n");
	if (!qf_usefile(&file_qf, LOCKS_FORBIDDEN, filename)) {
		fprintf(stderr, "Can't initialize the CQF from file: %s.\n", filename);
		abort();
	}
	for (uint64_t i = 0; i < nvals; i++) {
		uint64_t count = qf_count_key_value(&file_qf, vals[i], 0);
		if (count < key_count) {
			fprintf(stderr, "failed lookup in file based CQF for %lx %ld.\n",
							vals[i], count);
			abort();
		}
	}

	QFi qfi;
	/* Initialize an iterator */
	qf_iterator(&qf, &qfi, 0);
	do {
		uint64_t key, value, count;
		qfi_get(&qfi, &key, &value, &count);
		qfi_next(&qfi);
		if (qf_count_key_value(&qf, key, 0) < key_count) {
			fprintf(stderr, "Failed lookup during iteration for: %lx. Returned count: %ld\n",
							key, count);
			abort();
		}
	} while(!qfi_end(&qfi));

	fprintf(stdout, "Validated the CQF.\n");
}

