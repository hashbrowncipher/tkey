#!/bin/bash
set -o errexit
set -o nounset
set -o pipefail

# Put the path to your go binary in $1
export PATH="$1:$PATH"

pushd src/github.com/tillitis/tkey-device-signer
make clean
./build-podman.sh
popd

GOPATH="$(pwd)"
pushd src/github.com/tillitis/tillitis-key1-apps
echo $GOPATH
GOPATH=$GOPATH ./build.sh
popd

echo "Example invocation"
echo
echo "src/github.com/tillitis/tillitis-key1-apps/tkey-ssh-agent -a purpose1.sock --purpose purpose1 &"
echo "src/github.com/tillitis/tillitis-key1-apps/tkey-ssh-agent -a purpose2.sock --purpose purpose2 &"
echo "SSH_AUTH_SOCK=purpose1.sock ssh-add -L"
echo "SSH_AUTH_SOCK=purpose2.sock ssh-add -L"
