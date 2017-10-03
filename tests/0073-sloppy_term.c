/*
 * librdkafka - Apache Kafka C library
 *
 * Copyright (c) 2012-2015, Magnus Edenhill
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

#include "test.h"

/**
 * Sloppy termination where the application has not returned
 * all objects prior to calling destructor.
 * This makes sure the consumer close or destroy does not hang,
 * but the message and underlying ops, buffers, etc, will leak.
 */

#include "rdkafka.h"


int main_0073_sloppy_term (int argc, char **argv) {
        const char *topic = test_mk_topic_name(__FUNCTION__ + 5, 0);
        const int msgcnt = 1;
        rd_kafka_t *c;
        rd_kafka_message_t *rkmessage;
        rd_kafka_topic_partition_list_t *parts;

        if (!strcmp(test_mode, "valgrind")) {
                TEST_SKIP("This test deliberately leaks memory: "
                          "skipping valgrind run\n");
                return 0;
        }

        test_conf_init(NULL, NULL, 10);

        test_produce_msgs_easy(topic, 0, 0, msgcnt);

        c = test_create_consumer(topic, NULL, NULL, NULL);

        parts = rd_kafka_topic_partition_list_new(1);
        rd_kafka_topic_partition_list_add(parts, topic, 0)->offset =
                RD_KAFKA_OFFSET_BEGINNING;
        test_consumer_assign("assign", c, parts);
        rd_kafka_topic_partition_list_destroy(parts);

        rkmessage = rd_kafka_consumer_poll(c, tmout_multip(10*1000));
        TEST_ASSERT(rkmessage, "consumer_poll() timeout");
        TEST_ASSERT(!rkmessage->err, "consume error: %s",
                    rd_kafka_message_errstr(rkmessage));

        test_consumer_close(c);

        /* This will hang without the fix since rkmessage has not been
         * returned. */
        rd_kafka_destroy(c);

        return 0;
}
