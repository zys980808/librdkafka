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

#include "rdfuture.h"




rd_future_t *rd_futures_wait_any (rd_futures_t *rfuts, int timeout_ms) {
        rd_future_t *rfut;

        mtx_lock(&rfuts->rfuts_lock);
        while (!(rfut = TAILQ_FIRST(&rfuts->rfuts_doneq))) {
                if (cnd_timedwait_msp(&rfuts->rfuts_cond, &rfuts->rfuts_lock,
                                      &timeout_ms) == thrd_timedout) {
                        mtx_unlock(&rfuts->rfuts_lock);
                        return NULL;
                }
        }

        TAILQ_REMOVE(&rfuts->rfuts_doneq, rfut, rfut_setlink);
        mtx_unlock(&rfuts->rfuts_unlock);

        return rfut;
}


/**
 * @brief Wait for all futures in the set to complete.
 *
 * @returns 0 on completion or -1 on timeout.
 */
int rd_futures_wait_all (rd_futures_t *rfuts, int timeout_ms) {
        rd_future_t *rfut;

        mtx_lock(&rfuts->rfuts_lock);
        while (!TAILQ_EMPTY(&rfuts->rfuts_waitq)) {
                if (cnd_timedwait_msp(&rfuts->rfuts_cond, &rfuts->rfuts_lock,
                                      &timeout_ms) == thrd_timedout) {
                        mtx_unlock(&rfuts->rfuts_lock);
                        return NULL;
                }
        }
        mtx_unlock(&rfuts->rfuts_unlock);

        return 0;
}


void rd_futures_destroy (rd_futures_t *rfuts) {
        assert(TAILQ_EMPTY(&rfuts->rfuts_doneq));
        assert(TAILQ_EMPTY(&rfuts->rfuts_waitq));
        mtx_destroy(&rfuts->rfuts_lock);
        cnd_destroy(&rfuts->rfuts_cnd);
        if (rfuts->rfuts_allocated)
                rd_free(rfuts);
}

void rd_futures_init (rd_futures_t *rfuts) {
        mtx_init(&rfuts->rfuts_lock, mtx_plain);
        cnd_init(&rfuts->rfuts_cnd);
        TAILQ_INIT(&rfuts->rfuts_waitq);
        TAILQ_INIT(&rfuts->rfuts_doneq);
}

rd_futures_t *rd_futures_new (void) {
        rd_futures_t *rfuts = rd_calloc(1, sizeof(*rfuts));
        rd_futures_init(rfuts);
        rfuts->rfuts_allocated = 1;
        return rfuts;
}


void rd_futures_add (rd_futures_t *rfuts, rd_future_t *rfut) {
        mtx_lock(&rfut->rfut_lock);
        assert(!(rfut->rfut_flags & RD_FUTURE_F_CANCELLED));
        assert(!rfut->rfut_set);

        mtx_lock(&rfuts->rfuts_lock);

        rfut->rfut_set = rfuts;
        rfut->rfut_refcnt++;

        if (rfut->rfut_flags & RD_FUTURE_F_DONE) {
                TAILQ_INSERT_TAIL(&rfuts->rfuts_doneq, rfut, rfut_setlink);
                cnd_signal(&rfuts->rfuts_cnd);
        } else {
                TAILQ_INSERT_TAIL(&rfuts->rfuts_waitq, rfut, rfut_setlink);
        }
        mtx_unlock(&rfuts->rfuts_lock);
        mtx_unlock(&rfut->rfut_lock);
}


int rd_future_set_result (rd_future_t *rfut, void *result) {
        rd_futures_t *rfuts;
        void (*cb) (rd_future_t *rfut, void *result);

        mtx_lock(&rfut->rfut_lock);
        assert(!(rfut->rfut_flags & RD_FUTURE_F_DONE));

        if (unlikely((rfut->rfut_flags & RD_FUTURE_F_CANCELLED))) {
                if (!rd_future_destroy0(rfut))
                        mtx_unlock(&rfut->rfut_lock);
                return 0;
        }

        rfut->rfut_flags |= RD_FUTURE_F_DONE;
        rfut->rfut_result = result;

        if ((rfuts = rfut->rfut_set)) {
                mtx_lock(&rfuts->rfuts_lock);
                TAILQ_REMOVE(&rfuts->rfuts_waitq, rfut, rfut_setlink);
                TAILQ_INSERT_TAIL(&rfuts->rfuts_doneq, rfut, rfut_setlink);
                cnd_signal(&rfuts->rfuts_cnd);
                mtx_unlock(&rfuts->rfuts_lock);
        }

        /* FIXME: Should this be called with lock held? */
        if (rfut->rfut_cb)
                rfut->rfut_cb(rfut, result);

        if (!rd_future_destroy0(rfut))
                mtx_unlock(&rfut->rfut_lock);

        return 1;
}


/**
 * @brief Unlocked destroy
 * @returns 1 if rfut should be freed, else 0.
 */
static void rd_future_destroy0 (rd_future_t *rfut) {
        assert(rfut->rfut_refcnt > 0);
        if (--rfut->rfut_refcnt > 0)
                return 0;

        mtx_unlock(&rfut->rfut_lock); /* Caller's lock */
        assert(!rfut->rfut_set);
        mtx_destroy(&rfut->rfut_lock);
        cnd_destroy(&rfut->rfut_cnd);
        rd_free(rfut);

        return 1;
}



void *rd_future_cancel (rd_future_t *rfut) {
        void *result = NULL;
        rd_futures_t *rfuts;

        mtx_lock(&rfut->rfut_lock);

        if (rfut->rfut_flags & RD_FUTURE_F_DONE)
                result = rfut->rfut_result;

        if ((rfuts = rfut->rfut_set)) {
                mtx_lock(&rfuts->rfuts_lock);
                if (rfut->rfut_flags & RD_FUTURE_F_DONE) {
                        TAILQ_REMOVE(&rfuts->rfuts_doneq, rfut, rfut_setlink);
                        rfut->rfut_flags &= ~RD_FUTURE_F_DONE;
                } else
                        TAILQ_REMOVE(&rfuts->rfuts_waitq, rfut, rfut_setlink);
                cnd_signal(&rfuts->rfuts_cnd);
                mtx_unlock(&rfuts->rfuts_lock);

                rfut->rfut_set = NULL;
                if (rd_future_destroy0(rfut))
                        assert(!*"rd_future_t refcount mismatch");
        }

        rfut->rfut_flags |= RD_FUTURE_F_CANCELLED;

        if (!rd_future_destroy0(rfut))
                mtx_unlock(&rfut->rfut_lock);

        return result;
}


void *rd_future_result (rd_future_t *rfut) {
        void *result = NULL;

        mtx_lock(&rfut->rfut_lock);
        assert(!(rfut->rfut_flags & RD_FUTURE_F_CANCELLED));

        if (!(rfut->rfut_flags & RD_FUTURE_F_DONE)) {
                mtx_unlock(&rfut->rfut_lock);
                return NULL;
        }

        result = rfut->rfut_result;

        if (!rd_future_destroy0(rfut))
                mtx_unlock(&rfut->rfut_lock);

        return result;
}


void *rd_future_wait (rd_future_t *rfut, int timeout_ms) {
        void *result = NULL;
        rd_futures_t *rfuts;

        mtx_lock(&rfut->rfut_lock);
        cnd_timedwait_ms(&rfut->rfut_cnd, &rfut->rfut_lock, timeout_ms);
        assert(!(rfut->rfut_flags & RD_FUTURE_F_CANCELLED));

        if (!(rfut->rfut_flags & RD_FUTURE_F_DONE)) {
                mtx_unlock(&rfut->rfut_lock);
                return NULL;
        }

        if ((rfuts = rfut->rfut_set)) {
                mtx_lock(&rfuts->rfuts_lock);
                TAILQ_REMOVE(&rfuts->rfuts_doneq, rfut, rfut_setlink);
                mtx_unlock(&rfuts->rfuts_lock);

                if (rd_future_destroy0(rfut))
                        assert(!*"future_wait: invalid future refcount");
        }

        result = rfut->rfut_result;

        if (!rd_future_destroy0(rfut))
                mtx_unlock(&rfut->rfut_lock);

        return result;
}


rd_future_t *rd_future_new (void *opaque) {
        rd_future_t *rfut;

        rfut = rd_calloc(1, sizeof(*rfut));
        mtx_init(&rfut->rfut_lock, mtx_plain);
        cnd_init(&rfut->rfut_cnd);
        rfut->rfut_refcnt = 1;
        rfut->rfut_opaque = opaque;

        return rfut;
}
