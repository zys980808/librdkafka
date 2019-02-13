#!/bin/sh
#
#
# Build librdkafka on Alpine.
# Must only be run from within an Alpine container where
# the librdkafka root dir is mounted as /v
#

set -eu

if [ ! -f /.dockerenv ] ; then
    echo "$0 must be run in the docker container"
    exit 1
fi

apk add bash curl gcc g++ make musl-dev bsd-compat-headers git python perl

git clone /v /librdkafka

cd /librdkafka
./configure --install-deps --disable-gssapi --disable-lz4-ext
make
make -C tests run_local
cp -v src/librdkafka.so /v/artifacts/librdkafka-alpine.so
cd ..
