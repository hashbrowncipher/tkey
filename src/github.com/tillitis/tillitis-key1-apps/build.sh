#! /bin/sh

set -e

make -j -C ../tkey-libs
make -j -C ../tkey-device-signer

cp ../tkey-device-signer/signer/app.bin cmd/tkey-ssh-agent/app.bin

make -j
