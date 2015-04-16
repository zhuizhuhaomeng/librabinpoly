/*
 * Copyright (C) 2014 Steve Traugott (stevegt@t7a.org)
 * Copyright (C) 2013 Pavan Kumar Alampalli (pavankumar@cmu.edu)
 * Copyright (C) 2004 Hyang-Ah Kim (hakim@cs.cmu.edu)
 * Copyright (C) 1999 David Mazieres (dm@uun.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

/*
 * To use this library:
 *
 * XXX
 *
 *      rp_init() to get started
 *      rp_in() in a loop to pass input chunks into rabin algorithm
 *      rp_out() in a loop to get output blocks from rabin algorithm
 *      rp_reset() to start a new input stream
 *      rp_free() to free memory when done
 *
 * We maintain state by passing a RabinPoly to each function.
 *
 */

#include "rabinpoly.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

inline int rp_stream_read(RabinPoly *rp);
static inline void rp_stream_reset(RabinPoly *rp);
static inline void rp_inbuf_reset(RabinPoly *rp);
static inline void rp_block_reset(RabinPoly *rp);
static inline void rp_fragment_reset(RabinPoly *rp);

static inline int rp_noop(RabinPoly *rp) {return 0;}


/*
 * Routines for calculating the most significant bit of an integer.
 */

/* Highest bit set in a byte */
const char bytemsb[0x100] = {
  0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

/* Least set bit (ffs) */
const char bytelsb[0x100] = {
  0, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1, 5, 1, 2, 1, 3, 1, 2, 1,
  4, 1, 2, 1, 3, 1, 2, 1, 6, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1,
  5, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1, 7, 1, 2, 1, 3, 1, 2, 1,
  4, 1, 2, 1, 3, 1, 2, 1, 5, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1,
  6, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1, 5, 1, 2, 1, 3, 1, 2, 1,
  4, 1, 2, 1, 3, 1, 2, 1, 8, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1,
  5, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1, 6, 1, 2, 1, 3, 1, 2, 1,
  4, 1, 2, 1, 3, 1, 2, 1, 5, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1,
  7, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1, 5, 1, 2, 1, 3, 1, 2, 1,
  4, 1, 2, 1, 3, 1, 2, 1, 6, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1,
  5, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1,
};

/* Find last set (most significant bit) */
static inline u_int fls32 (u_int32_t v)
{
  if (v & 0xffff0000) {
    if (v & 0xff000000)
      return 24 + bytemsb[v>>24];
    else
      return 16 + bytemsb[v>>16];
  }
  if (v & 0x0000ff00)
    return 8 + bytemsb[v>>8];
  else
    return bytemsb[v];
}

//static inline u_int fls64 (u_int64_t) __attribute__ ((const));
static inline char fls64 (u_int64_t v)
{
  u_int32_t h;
  if ((h = v >> 32))
    return 32 + fls32 (h);
  else
    return fls32 ((u_int32_t) v);
}

// static inline int log2c64 (u_int64_t) __attribute__ ((const));
// static inline int
// log2c64 (u_int64_t v)
// {
//      return v ? (int) fls64 (v - 1) : -1;
// }

#define fls(v) (sizeof (v) > 4 ? fls64 (v) : fls32 (v))
// #define log2c(v) (sizeof (v) > 4 ? log2c64 (v) : log2c32 (v))

/*
 * For symmetry, a 64-bit find first set, "ffs," that finds the least
 * significant 1 bit in a word.
 */

// static inline u_int
// ffs32 (u_int32_t v)
// {
//   int vv;
//   if (v & 0xffff) {
//     if ((vv = (v & 0xff)))
//       return bytelsb[vv];
//     else
//       return 8 + bytelsb[v >> 8 & 0xff];
//   }
//   else if ((vv = (v & 0xff0000)))
//     return 16 + bytelsb[vv >> 16];
//   else if (v)
//     return 24 + bytelsb[v >> 24 & 0xff];
//   else
//     return 0;
// }
// 
// static inline u_int
// ffs64 (u_int64_t v)
// {
//   u_int32_t l;
//   if ((l = v & 0xffffffff))
//     return fls32 (l);
//   else if ((l = v >> 32))
//     return 32 + fls32 (l);
//   else
//     return 0;
// }
// 
// #define ffs(v) (sizeof (v) > 4 ? ffs64 (v) : ffs32 (v))


#define INT64(n) n##LL
#define MSB64 INT64(0x8000000000000000)

#define DEFAULT_WINDOW_SIZE 32

/* Fingerprint value take from LBFS fingerprint.h. For detail on this, 
 * refer to the original rabin fingerprint paper.
 */
#define FINGERPRINT_PT 0xbfe6b8a5bf378d83LL	

static u_int64_t polymod (u_int64_t nh, u_int64_t nl, u_int64_t d);
static void polymult (u_int64_t *php, u_int64_t *plp, u_int64_t x, u_int64_t y);
static u_int64_t polymmult (u_int64_t x, u_int64_t y, u_int64_t d);

static void calcT(RabinPoly *rp);
static u_int64_t slide8(RabinPoly *rp, unsigned char m);
static u_int64_t append8(RabinPoly *rp, u_int64_t p, unsigned char m);

/*
     functions to calculate the rabin hash
*/

static u_int64_t polymod (u_int64_t nh, u_int64_t nl, u_int64_t d) {
	int i;
	int k = fls64 (d) - 1;
	d <<= 63 - k;

	if (nh) {
		if (nh & MSB64)
			nh ^= d;
		for (i = 62; i >= 0; i--)
			if (nh & ((u_int64_t) 1) << i) {
				nh ^= d >> (63 - i);
				nl ^= d << (i + 1);
			}
	}
	for (i = 63; i >= k; i--)
	{  
		if (nl & INT64 (1) << i)
			nl ^= d >> (63 - i);
	}
	return nl;
}

static void polymult (
        u_int64_t *php, u_int64_t *plp, u_int64_t x, u_int64_t y) {
	int i;
	u_int64_t ph = 0, pl = 0;
	if (x & 1)
		pl = y;
	for (i = 1; i < 64; i++)
		if (x & (INT64 (1) << i)) {
			ph ^= y >> (64 - i);
			pl ^= y << i;
		}
	if (php)
		*php = ph;
	if (plp)
		*plp = pl;
}

static u_int64_t polymmult (u_int64_t x, u_int64_t y, u_int64_t d) {
	u_int64_t h, l;
	polymult (&h, &l, x, y);
	return polymod (h, l, d);
}

/*
    Initialize the T[] and U[] array for faster computation of rabin
    fingerprint.  Called only once from rp_init() during
    initialization.
 */

static void calcT(RabinPoly *rp) { 
    unsigned int i; 
    int xshift = fls64 (rp->poly) - 1; 
    rp->shift = xshift - 8;

	u_int64_t T1 = polymod (0, INT64 (1) << xshift, rp->poly);
	for (i = 0; i < 256; i++) {
		rp->T[i] = polymmult (i, T1, rp->poly) | ((u_int64_t) i << xshift);
	}

	u_int64_t sizeshift = 1;
	for (i = 1; i < rp->window_size; i++) {
		sizeshift = append8 (rp, sizeshift, 0);
	}

	for (i = 0; i < 256; i++) {
		rp->U[i] = polymmult (i, sizeshift, rp->poly);
	}
}

/*
   Feed a new byte into the rabin sliding window and update the rabin
   fingerprint.
 */

static u_int64_t slide8(RabinPoly *rp, unsigned char m) { 
    rp->circbuf_pos++;
	if (rp->circbuf_pos >= rp->window_size) {
		rp->circbuf_pos = 0;
	}
	unsigned char om = rp->circbuf[rp->circbuf_pos];
	rp->circbuf[rp->circbuf_pos] = m;
	return rp->fingerprint = append8 (rp, rp->fingerprint ^ rp->U[om], m);
}

static u_int64_t append8(RabinPoly *rp, u_int64_t p, unsigned char m) { 	
	return ((p << 8) | m) ^ rp->T[p >> rp->shift]; 
}


/*
 
    rp_init() -- Initialize the RabinPoly structure
  
    Call this first to create a state container to be passed to the
    other library functions.  You'll later need to free all of the
    memory associated with this container by passing it to
    rp_free(). 

    Args:
    -----

    window_size
                
        Rabin fingerprint window size in bytes.  Suitable values range
        from 32 to 128.

    avg_block_size 
                
        Average block size in bytes
  
    min_block_size 

        Minimum block size in bytes
  
    max_block_size 

        Maximum block size in bytes
  

    Return values:
    --------------
  
    rp 
        Pointer to the RabinPoly structure we've allocated
  
    NULL 
        Either malloc or arg error XXX need better error codes

*/

RabinPoly* rp_new(unsigned int window_size,
						size_t avg_block_size, 
						size_t min_block_size,
						size_t max_block_size,
                        size_t inbuf_size) {
	RabinPoly *rp;

	if (!min_block_size || !avg_block_size || !max_block_size ||
		(min_block_size > avg_block_size) ||
		(max_block_size < avg_block_size) ||
		(window_size < DEFAULT_WINDOW_SIZE)) {
		return NULL;
	}

	rp = (RabinPoly *)malloc(sizeof(RabinPoly));
	if (!rp) {
		return NULL;
	}

	rp->poly = FINGERPRINT_PT;
	rp->window_size = window_size;
	rp->inbuf_size = inbuf_size;
	rp->avg_block_size = avg_block_size;
	rp->min_block_size = min_block_size;
	rp->max_block_size = max_block_size;
	rp->fingerprint_mask = (1 << (fls32(rp->avg_block_size)-1))-1;

    rp->func_block_start = rp_noop;
    rp->func_block_end = rp_noop;
    rp->func_fragment_end = rp_noop;
    rp->func_stream_read = rp_stream_read;

	rp->circbuf = (unsigned char *)malloc(rp->window_size*sizeof(unsigned char));
	if (!rp->circbuf){
        free(rp);
		return NULL;
	}

	rp->inbuf = (unsigned char *)malloc(rp->inbuf_size*sizeof(unsigned char));
	if (!rp->inbuf){
        free(rp->circbuf);
        free(rp);
		return NULL;
	}

    rp_stream_reset(rp);

    calcT(rp);

    return rp;
}

void rp_free(RabinPoly *rp)
{
	if (!rp) {
		return;
	}
	free(rp->inbuf);
	free(rp->circbuf);
	free(rp);
	rp = NULL;
}


static inline void rp_stream_reset(RabinPoly *rp) { 
	rp->fingerprint = 0; 
	rp->circbuf_pos = -1;
	bzero ((char*) rp->circbuf, rp->window_size*sizeof (unsigned char));
	rp->block_streampos = 0;
    rp_inbuf_reset(rp);
    rp_block_reset(rp);
}

static inline void rp_inbuf_reset(RabinPoly *rp) { 
    rp->inbuf_pos = 0;
    rp->inbuf_read_count = 0;
    rp->fragment_addr = rp->inbuf;
    rp_fragment_reset(rp);
}

static inline void rp_block_reset(RabinPoly *rp) { 
    rp->block_streampos += rp->block_size;
	rp->block_size = 0;
}

static inline void rp_fragment_reset(RabinPoly *rp) { 
	rp->fragment_size = 0;
    rp->fragment_addr = rp->inbuf + rp->inbuf_pos;
}

inline int rp_stream_read(RabinPoly *rp) {
    rp->error = 0;
    rp->inbuf_read_count = fread(rp->inbuf, 1, rp->inbuf_size, rp->stream);
    if (rp->inbuf_read_count == 0) {
        if (ferror(rp->stream)) {
            rp->error = errno;
        } else if (feof(rp->stream)) {
            rp->error = EOF;
        }
    }
    return rp->error;
}


#define STREAM_READ      1
#define BLOCK_START      2
#define BLOCK_END        4
#define FRAGMENT_START   8
#define FRAGMENT_END    16
#define STOP            32

int rp_stream_process(RabinPoly *rp, FILE *stream) {

    rp_stream_reset(rp);
    rp->stream = stream;

    int rc, read_rc;
    int state = STREAM_READ | BLOCK_START | FRAGMENT_START;
    for (;;) {

        if (state & FRAGMENT_END) {
            state ^= FRAGMENT_END;
            rc = rp->func_fragment_end(rp);
            if (rc) return rc;
            state |= FRAGMENT_START;
        }

        if (state & BLOCK_END) {
            state ^= BLOCK_END;
            rc = rp->func_block_end(rp);
            if (rc) return rc;
            state |= BLOCK_START;
        }

        if (state & STOP) {
            return read_rc;
        }

        if (state & STREAM_READ) {
            state ^= STREAM_READ;
            rp_inbuf_reset(rp);
            read_rc = rp->func_stream_read(rp);
            if (read_rc) {
                state |= FRAGMENT_END | BLOCK_END | STOP;
            }
        }

        if (state & FRAGMENT_START) {
            state ^= FRAGMENT_START;
            rp_fragment_reset(rp);
        }

        if (state & BLOCK_START) {
            state ^= BLOCK_START;
            rp_block_reset(rp);
            rc = rp->func_block_start(rp);
            if (rc) return rc;
            /* 
             * Skip early part of each block -- there appears to be no reason
             * to checksum the first min_block_size-N bytes, because the
             * effect of those early bytes gets flushed out pretty quickly.
             * Setting N to 256 seems to work; not sure if that's the "right"
             * number, but we'll use that for now.  This one optimization
             * alone provides a 30% speedup in benchmark.py though, with no
             * detected change in block boundary locations or fingerprints in
             * any of the existing tests.  - stevegt
             *
             * @moinakg found similar results, and also seems to think 256 is
             * right: https://moinakg.wordpress.com/tag/rolling-hash/
             *
            */
            size_t skip = rp->min_block_size - 256;
            if (((rp->inbuf_read_count - rp->inbuf_pos) > rp->min_block_size+1) && 
                    (rp->min_block_size > 512)) {
                rp->inbuf_pos += skip;
                rp->block_size += skip;
                rp->fragment_size += skip;
            }
        }

        if (state) {
            continue;
        }

        while (!state) {

            if (rp->inbuf_pos == rp->inbuf_read_count) {
                /* end of input buffer */
                state |= FRAGMENT_END | STREAM_READ;
                break;
            }

            if (rp->block_size == rp->max_block_size) {
                /* full block */
                state |= FRAGMENT_END | BLOCK_END;
                break;
            }

            /* feed the next byte into rabinpoly algo */
            slide8(rp, rp->inbuf[rp->inbuf_pos]);
            rp->inbuf_pos++;
            rp->block_size++;
            rp->fragment_size++;

            /* 
             *  
             * We compare the low-order fingerprint bits (LOFB) to
             * something other than zero in order to avoid generating
             * short blocks when scanning long strings of zeroes.
             * Mechiel Lukkien (mjl), while working on the Plan9 gsoc,
             * seemed to think that avg_block_size - 1 was a good value.
             * 
             * http://gsoc.cat-v.org/people/mjl/blog/2007/08/06/1_Rabin_fingerprints/ 
             * 
             * ...and since we're already using avg_block_size - 1 to set
             * the fingerprint mask itself, then simply comparing LOFB to
             * the mask itself will do the right thing.  
             *  
             */
            if ((rp->block_size >= rp->min_block_size) &&
                ((rp->fingerprint & rp->fingerprint_mask) == rp->fingerprint_mask)) {
                /* fingerprint boundary found */
                state |= FRAGMENT_END | BLOCK_END;
            }
        }
    }
}



/* 
 
    rp_in() -- Input data into the rabinpoly algorithm 

    See rp_out() synopsis.

    Args:
    -----

    rp       

        Pointer to the RabinPoly structure from rp_init.

    buf      

        Pointer to input string buffer.

    size    

        Max number of bytes we're supposed to read from buf.  This
        might be less than buffer size, e.g. if caller is using a
        fixed-length buffer but has loaded a shorter string into it.

    eof

        Caller has reached end of file.  Caller sets this to indicate
        that we must mark the final block as done regardless of
        polynomial state.


    Return values:
    --------------

    0

        All is well.

    -1 

        Invalid argument.
  
 */

/* 
 
    rp_out() -- Generate output from the rabinpoly algorithm


    Synopsis (in pseudo code):
    --------------------------

	XXX

        rp = rp_init(...)
		hash.reset()
        while buf = file.read(size):
            eof = len(buf) < size
            rp_in(rp, buf, size, eof)
            while rp_out(rp):
                hash.append(rp->fragment_start, rp->fragment_size)
                if rp->block_done:
                    do something with rp->block_start
                    do something with rp->block_size
                    do something with hash.digest()
                    hash.reset()
			if rp->eof:
				break

    Each call to rp_out() returns a boolean indicating whether or
    not we have processed all the bytes in our input buffer. 

    When we find a block boundary, then we return 1.  We set
    rp->block_done to 1, and rp->block_size to the total block length
    in bytes.  We will never return a block length longer than
    max_block_size, or shorter than min_block_size.  We will attempt
    to return blocks with an average length of avg_block_size.

    If caller passed an incomplete block to rp_in() and did not set
    eof, then we return 0.  We don't set rp->block_done.  We set
    rp->block_size to the subtotal of all fragments so far in this
    block.

    if caller passed eof=1 to rp_in(), then we set rp->eof when we
    return the last block.

    We always set rp->fragment_start and rp->fragment_size, to aid the caller
    in hashing our results.  (If the fragment contains a complete
    block, then rp->fragment_size == rp->block_size.)


    Args:
    -----

    rp       

        Pointer to the RabinPoly structure from rp_init.


    Return values:
    --------------

	Return value is a bitmask composed of:

	RP_IN

		Caller must load rp->inbuf with new input data by calling
		rp_in(), then call rp_out() again.  

	RP_PROCESS_FRAGMENT

		Caller must process the current fragment (e.g. update block
		hash), then call rp_out() again.  

	RP_PROCESS_BLOCK

		Caller must process the completed block (e.g. finalize block
		hash and do something with it), then call rp_out()
		again.  

	RP_RESET

		End of stream.  Caller should clean up (close files etc.) and
		must not call rp_out() again without a rp_reset() first.
			
 */
