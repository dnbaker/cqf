/*
 * =====================================================================================
 *
 *       Filename:  gqf.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2018-03-21 10:43:39 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prashant Pandey (), ppandey@cs.stonybrook.edu
 *   Organization:  Stony Brook University
 *
 * =====================================================================================
 */

#ifndef _GQF_H_
#define _GQF_H_

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>

#include "include/gqf_file.h"

#ifdef __cplusplus
extern "C" {
#endif

	/* Can be 
		 0 (choose size at run-time), 
		 8, 16, 32, or 64 (for optimized versions),
		 or other integer <= 56 (for compile-time-optimized bit-shifting-based versions)
		 */
#define BITS_PER_SLOT 0

#define BITMASK(nbits) (0xffffffffffffffff >> (64 - (nbits)))
#define MAX_VALUE(nbits) BITMASK(nbits)

	/* Must be >= 6.  6 seems fastest. */
#define BLOCK_OFFSET_BITS (6)

#define SLOTS_PER_BLOCK (1ULL << BLOCK_OFFSET_BITS)
#define METADATA_WORDS_PER_BLOCK ((SLOTS_PER_BLOCK + 63) / 64)

#define NUM_SLOTS_TO_LOCK (1ULL<<16)
#define CLUSTER_SIZE (1ULL<<14)

#define METADATA_WORD(qf,field,slot_index) (get_block((qf), (slot_index) / \
																											SLOTS_PER_BLOCK)->field[((slot_index)  % SLOTS_PER_BLOCK) / 64])

#define BILLION 1000000000L

	typedef struct __attribute__ ((__packed__)) qfblock {
		/* Code works with uint16_t, uint32_t, etc, but uint8_t seems just as fast as
		 * anything else */
		uint8_t offset; 
		uint64_t occupieds[METADATA_WORDS_PER_BLOCK];
		uint64_t runends[METADATA_WORDS_PER_BLOCK];

#if BITS_PER_SLOT == 8
		uint8_t  slots[SLOTS_PER_BLOCK];
#elif BITS_PER_SLOT == 16
		uint16_t  slots[SLOTS_PER_BLOCK];
#elif BITS_PER_SLOT == 32
		uint32_t  slots[SLOTS_PER_BLOCK];
#elif BITS_PER_SLOT == 64
		uint64_t  slots[SLOTS_PER_BLOCK];
#elif BITS_PER_SLOT != 0
		uint8_t   slots[SLOTS_PER_BLOCK * BITS_PER_SLOT / 8];
#else
		uint8_t   slots[];
#endif
	} qfblock;

	struct __attribute__ ((__packed__)) qfblock;
	typedef struct qfblock qfblock;

	enum hashmode {
		DEFAULT,
		INVERTIBLE,
		NONE
	};

	enum lockingmode {
		LOCKS_FORBIDDEN,
		LOCKS_OPTIONAL,
		LOCKS_REQUIRED
	};

	// The below struct is used to instrument the code.
	// It is not used in normal operations of the CQF.
	typedef struct {
		uint64_t total_time_single;
		uint64_t total_time_spinning;
		uint64_t locks_taken;
		uint64_t locks_acquired_single_attempt;
	} wait_time_data;

	typedef struct quotient_filter_runtime_data {
		file_info f_info;
		uint64_t num_locks;
		enum lockingmode lock_mode;
		volatile int metadata_lock;
		volatile int *locks;
		wait_time_data *wait_times;
	} quotient_filter_runtime_data;

	typedef quotient_filter_runtime_data qfruntime;

	typedef struct quotient_filter_metadata {
		enum hashmode hash_mode;
		uint32_t auto_resize;
		uint64_t total_size_in_bytes;
		uint32_t seed;
		uint64_t nslots;
		uint64_t xnslots;
		uint64_t key_bits;
		uint64_t value_bits;
		uint64_t key_remainder_bits;
		uint64_t bits_per_slot;
		__uint128_t range;
		uint64_t nblocks;
		uint64_t nelts;
		uint64_t ndistinct_elts;
		uint64_t noccupied_slots;
	} quotient_filter_metadata;

	typedef quotient_filter_metadata qfmetadata;

	typedef struct quotient_filter {
		qfruntime *runtimedata;
		qfmetadata *metadata;
		qfblock *blocks;
	} quotient_filter;

	typedef quotient_filter QF;

	// The below struct is used to instrument the code.
	// It is not used in normal operations of the CQF.
	typedef struct {
		uint64_t start_index;
		uint16_t length;
	} cluster_data;

	typedef struct quotient_filter_iterator {
		const QF *qf;
		uint64_t run;
		uint64_t current;
		uint64_t cur_start_index;
		uint16_t cur_length;
		uint32_t num_clusters;
		cluster_data *c_info;
	} quotient_filter_iterator;

	typedef quotient_filter_iterator QFi;

	/* Forward declaration for the macro. */
	void qf_dump_metadata(const QF *qf);

#define DEBUG_CQF(fmt, ...) \
	do { if (PRINT_DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

#define DEBUG_DUMP(qf) \
	do { if (PRINT_DEBUG) qf_dump_metadata(qf); } while (0)

	/* Initialize the CQF at "buffer".
	 * If there is not enough space at buffer then it will return the total size
	 * needed in bytes to initialize the CQF.
	 * */
	uint64_t qf_init(QF *qf, uint64_t nslots, uint64_t key_bits, uint64_t
									 value_bits, enum lockingmode lock, enum hashmode hash,
									 uint32_t seed, void* buffer, uint64_t buffer_len);

	/* Read the CQF stored at "buffer". */
	uint64_t qf_use(QF* qf, void* buffer, uint64_t buffer_len, enum lockingmode
									lock);

	void *qf_destroy(QF *qf);

	/* Initialize the CQF and allocate memory for the CQF. */
	bool qf_malloc(QF *qf, uint64_t nslots, uint64_t key_bits, uint64_t
								 value_bits, enum lockingmode lock, enum hashmode hash,
								 uint32_t seed);

	bool qf_free(QF *qf);

	void qf_reset(QF *qf);

	/* The caller should call qf_init on the dest QF using the same parameters
	 * as the src QF before calling this function. */
	void qf_copy(QF *dest, const QF *src);

	/* Allocate a new CQF using "nslots" and copy elements from "qf" into it. */
	bool qf_resize_malloc(QF *qf, uint64_t nslots);

	/* Allocate a new CQF using "nslots" at "buffer" and copy elements from "qf"
	 * into it. 
	 * If there is not enough space at buffer then it will return the total size
	 * needed in bytes to initialize the new CQF.
	 * */
	uint64_t qf_resize(QF* qf, uint64_t nslots, void* buffer, uint64_t
										 buffer_len);

	void qf_set_auto_resize(QF* qf);

	/* Increment the counter for this key/value pair by count. */
	bool qf_insert(QF *qf, uint64_t key, uint64_t value, uint64_t count);

	/* Set the counter for this key/value pair to count. */
	bool qf_set_count(QF *qf, uint64_t key, uint64_t value, uint64_t count);

	/* Remove count instances of this key/value combination. */
	bool qf_remove(QF *qf, uint64_t key, uint64_t value, uint64_t count);

	/* Remove all instances of this key/value pair. */
	bool qf_delete_key_value(QF *qf, uint64_t key, uint64_t value);

	/* Remove all instances of this key. */
	void qf_delete_key(QF *qf, uint64_t key);

	/* Replace the association (key, oldvalue, count) with the association
		 (key, newvalue, count). If there is already an association (key,
		 newvalue, count'), then the two associations will be merged and
		 their counters will be summed, resulting in association (key,
		 newvalue, count' + count). */
	void qf_replace(QF *qf, uint64_t key, uint64_t oldvalue, uint64_t newvalue);

	/* Lookup the value associated with key.  Returns the count of that
		 key/value pair in the QF.  If it returns 0, then, the key is not
		 present in the QF. Only returns the first value associated with key
		 in the QF.  If you want to see others, use an iterator. */
	uint64_t qf_query(const QF *qf, uint64_t key, uint64_t *value);

	/* Return the number of times key has been inserted, with any value,
		 into qf. */
	uint64_t qf_count_key(const QF *qf, uint64_t key);

	/* Return the number of times key has been inserted, with the given
		 value, into qf. */
	uint64_t qf_count_key_value(const QF *qf, uint64_t key, uint64_t value);

	/* Initialize an iterator */
	bool qf_iterator(const QF *qf, QFi *qfi, uint64_t position);

	/* Initialize an iterator and position it at the smallest index containing a
	 * hash value greater than or equal to "hash". */
	bool qf_iterator_hash(const QF *qf, QFi *qfi, uint64_t hash);

	/* Returns 0 if the iterator is still valid (i.e. has not reached the
		 end of the QF. */
	int qfi_get(const QFi *qfi, uint64_t *key, uint64_t *value, uint64_t *count);

	/* Advance to next entry.  Returns whether or not another entry is
		 found.  */
	int qfi_next(QFi *qfi);

	/* Check to see if the if the end of the QF */
	int qfi_end(const QFi *qfi); 

	/* For debugging */
	void qf_dump(const QF *);

	/* mmap the QF from disk. */
	void qf_read(QF *qf, const char *path);

	/* merge two QFs into the third one. */
	void qf_merge(QF *qfa, QF *qfb, QF *qfc);

	/* merge multiple QFs into the final QF one. */
	void qf_multi_merge(const QF *qf_arr[], int nqf, QF *qfr);

	/* find cosine similarity between two QFs. */
	uint64_t qf_inner_product(QF *qfa, QF *qfb);

	/* magnitude of a QF. */
	uint64_t qf_magnitude(QF *qf);

#ifdef __cplusplus
}
#include <array>
#include <cstring>
namespace qf {
class filter {
    QF filt_;
    class iterator {
        QFi it_;
        using TupleType = std::array<uint64_t, 3>;
    public:
        iterator(const filter *filter, uint64_t pos) {
            qf_iterator((const QF *)filter, &static_cast<QFi &>(*this), pos);
        }
        iterator &operator++() {qfi_next(&(QFi &)(*this)); return *this;}
        iterator operator++(int) {
            iterator ret(*this);
            operator++();
            return ret;
        }
        operator QFi &()       {return *(QFi *)this;}
        operator const QFi &() const {return *(const QFi *)this;}
        iterator(const iterator &it) {
            std::memcpy(this, &it, sizeof(it));
        }
        int get(uint64_t *key, uint64_t *value, uint64_t *count) const {
#ifdef __EXCEPTIONS
            if(qfi_get(&static_cast<const QFi &>(*this), key, value, count)) throw std::runtime_error("Iterator is exhausted.");
            return 0;
#else
            return qfi_get(&static_cast<QFi &>(*this), key, value, count);
#endif
        }
        TupleType get() const {
            std::array<uint64_t, 3> ret;
            get(ret);
            return ret;
        }
        TupleType operator*() const {return get();}
        void get(TupleType &tup) const { // Modify tuple in-place.
            get(&tup[0], &tup[1], &tup[2]);
        }
        template<typename T>
        bool operator==([[maybe_unused]] const T &val) const {
            return end();
        }
        template<typename T>
        bool operator!=([[maybe_unused]] const T &val) const {
            return !operator==(val);
        }
        int end() const {
            return qfi_end(&static_cast<const QFi &>(*this));
        }
    };
public:
    operator QF &() {
        return *(QF *)this;
    }
    operator const QF &() {
        return *(const QF *)this;
    }
    filter(uint64_t nslots, uint64_t key_bits, uint64_t value_bits) {
        qf_init(&filt_, nslots, key_bits, value_bits);
    }
    filter(const char *filename) {
        qf_deserialize(&filt_, filename);
    }
    iterator begin() {
        return iterator(this, 0);
    }
    iterator end() const {
        return iterator(this, 0);
    }
    void insert(uint64_t key, uint64_t value, uint64_t count) {
        qf_insert(&filt_, key % filt_.range, value, count);
    }
    void remove(uint64_t key, uint64_t value, uint64_t count) {
        qf_remove(&filt_, key % filt_.range, value, count);
    }
    uint64_t count(uint64_t key) const {
        return qf_count_key(&filt_, key % filt_.range);
    }
    uint64_t count(uint64_t key, uint64_t value) const {
        return qf_count_key_value(&filt_, key, value);
    }
    void dump() const {
        qf_dump(&filt_);
    }
    void query(uint64_t key, uint64_t *value) const {
        qf_query(&filt_, key, value);
    }
    uint64_t query(uint64_t key) const {
        uint64_t ret;
        query(key, &ret);
        return ret;
    }
    void replace(uint64_t key, uint64_t oldvalue, uint64_t newvalue) {
        qf_replace(&filt_, key, oldvalue, newvalue);
    }
    void del(uint64_t key) {
        qf_delete_key(&filt_, key);
    }
    void del(uint64_t key, uint64_t value) {
        qf_delete_key_value(&filt_, key, value);
    }
    ~filter() {
        qf_destroy(&filt_);
    }
};
} // namespace qf
#endif

#endif /* _GQF_H_ */
