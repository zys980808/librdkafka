/*
 * librdkafka - Apache Kafka C library
 *
 * Copyright (c) 2016, Magnus Edenhill
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once



typedef struct rd_future_s {
        TAILQ_ENTRY(rd_future_s) rfut_setlink;
        struct rd_futures_s *rfut_set;
        mtx_t  rfut_lock;
        cnd_t  rfut_cnd;
        int    rfut_refcnt;
        int    rfut_flags;
#define RD_FUTURE_F_CANCELLED 0x1
#define RD_FUTURE_F_DONE      0x2
        void  *rfut_result;
        void (*rfut_cb) (struct rd_future_s *rfut,
                         void *result, void *opaque);
        void  *rfut_opaque;
} rd_future_t;


typedef struct rd_futures_s {
        mtx_t  rfuts_lock;
        cnd_t  rfuts_cnd;

        TAILQ_HEAD(, rd_future_s) rfuts_doneq;
        TAILQ_HEAD(, rd_future_s) rfuts_waitq;

        int    rfuts_allocated;
} rd_futures_t;



/**
 * @brief Future
 *
 *
 */


/**
 * @brief Create new future.
 */
rd_future_t *rd_future_new (void *opaque);


/**
 * @brief Return result for future (if done)
 *
 * @returns result pointer on success in which case the future is destroyed,
 *          or NULL if future is not completed yet.
 */
void *rd_future_result (rd_future_t *rfut);


/**
 * @brief Wait for future to complete
 *
 * @remark cancel() and wait() are mutually exclusive.
 *
 * @returns result pointer on success in which case the future is destroyed,
 *          or NULL on timeout (future will not be destroyed)
 */
void *rd_future_wait (rd_future_t *rfut, int timeout_ms);

/**
 * @brief Cancel and destroy a future.
 *
 * If the future has already completed the result will be returned, else NULL.
 */
void *rd_future_cancel (rd_future_t *rfut);


/**
 * @brief Set future's result
 *
 * @returns 1 if result was set, or 0 if future has already been cancelled
 *          in which case the caller is responsible for \p result.
 */
int rd_future_set_result (rd_future_t *rfut, void *result);






 /**
 * @brief Future-sets provide efficient awaitiable collections of futures
 *
 *
 */

/**
 * @brief Destroy (an empty) future-set
 */
void rd_futures_destroy (rd_futures_t *rfuts);

/**
 * @brief Initialize an empty future-set
 */
void rd_futures_init (rd_futures_t *rfuts);

/**
 * @brief Create and initialize an empty future-set
 */
rd_futures_t *rd_futures_new (void);

/**
 * @brief Add future to future-set's wait queue
 */
void rd_futures_add (rd_futures_t *rfuts, rd_future_t *rfut);

/**
 * @brief Await future to complete in future-set
 *
 * @returns completed future on success or NULL on timeout
 */
rd_future_t *rd_futures_wait_any (rd_futures_t *rfuts, int timeout_ms);

/**
 * @brief Wait for all futures in the set to complete.
 *
 * @returns 0 on completion or -1 on timeout.
 */
int rd_futures_wait_all (rd_futures_t *rfuts, int timeout_ms);
