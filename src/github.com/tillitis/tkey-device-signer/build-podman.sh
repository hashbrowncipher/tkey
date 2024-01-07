#! /bin/sh

# Copyright (C) 2023 - Tillitis AB
# SPDX-License-Identifier: GPL-2.0-only

set -e

make -j -C ../tkey-libs podman

make -j podman
