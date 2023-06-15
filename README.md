
[![ci](https://github.com/tillitis/tkey-devtools/actions/workflows/ci.yaml/badge.svg?branch=main&event=push)](https://github.com/tillitis/tkey-devtools/actions/workflows/ci.yaml)

# tkey-tools

This repository contains some development tools for the
[Tillitis](https://tillitis.se/) TKey USB security stick.

- `tkey-runapp`: A simple development tool to load and start any TKey
  device app.

- `run-tkey-qemu`: Script around our 
  [TKey emulator](https://github.com/tillitis/qemu) OCI image
  `ghcr.io/tillitis/tkey-qemu-tk1-23.03.1`.

See the [TKey Developer Handbook](https://dev.tillitis.se/) for how to
develop your own apps, how to run and debug them in the emulator or on
real hardware.

[Current list of known projects](https://dev.tillitis.se/projects/).

## Building

You have two options, either our OCI image
`ghcr.io/tillitis/tkey-builder` for use with a rootless podman setup,
or native tools. See [the Devoloper
Handbook](https://dev.tillitis.se/) for setup.

With native tools you should be able to use make

```
$ make
```

If you want to use podman and you have `make` you can run:

```
$ podman pull ghcr.io/tillitis/tkey-builder:2
$ make podman
```

or run podman directly with

```
$ podman run --rm --mount type=bind,source=.,target=/src -w /src -it ghcr.io/tillitis/tkey-builder:2 make -j
```

To help prevent unpleasant surprises we keep a hash of the `signer` in
`cmd/tkey-ssh-agent/app.bin.sha512`. The compilation will fail if this
is not the expected binary.

### Using tkey-runapp

The client app `tkey-runapp` only loads and starts a device app. It's
mostly a development tool. You'll then have to switch to a different
client app that speaks your app's specific protocol. Run with `-h` to
get help.

### Using run-tkey-qemu

```
$ ./run-tkey-qemu
```

This gives you `tkey-qemu-pty` in the current working directory you
can attach your client programs to, typically with `--port
./tkey-qemu-pty`.

## Licenses and SPDX tags

Unless otherwise noted, the project sources are licensed under the
terms and conditions of the "GNU General Public License v2.0 only":

> Copyright Tillitis AB.
>
> These programs are free software: you can redistribute it and/or
> modify it under the terms of the GNU General Public License as
> published by the Free Software Foundation, version 2 only.
>
> These programs are distributed in the hope that it will be useful,
> but WITHOUT ANY WARRANTY; without even the implied warranty of
> MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
> General Public License for more details.

> You should have received a copy of the GNU General Public License
> along with this program. If not, see:
>
> https://www.gnu.org/licenses

See [LICENSE](LICENSE) for the full GPLv2-only license text.

External source code we have imported are isolated in their own
directories. They may be released under other licenses. This is noted
with a similar `LICENSE` file in every directory containing imported
sources.

The project uses single-line references to Unique License Identifiers
as defined by the Linux Foundation's [SPDX project](https://spdx.org/)
on its own source files, but not necessarily imported files. The line
in each individual source file identifies the license applicable to
that file.

The current set of valid, predefined SPDX identifiers can be found on
the SPDX License List at:

https://spdx.org/licenses/

All contributors must adhere to the [Developer Certificate of Origin](dco.md).
