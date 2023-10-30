# RBTHS Clients

https://rbths.com

Access, Observability & Security.
All In One Suite for IoT devices.

## Overview

This repo contains the majority of Rbths' open source code. 

Rbths.com, (stands for RabbitHoles in case anyone is wondering), provides an AIO suite of IoT devInfra to provide Access, Observability & Security to smart devices around the world.

At its core, there are 2 pieces of software that enables RBTHS suites.

1. [Tailscale](https://github.com/tailscale/tailscale) based network, which connects all IoT devices one org manages into one or more virtual network.
2. [LogIndexer](https://github.com/rbths/rbths-clients/tree/main/exec/log_indexer), a gRPC based server responing to log related queries on IoT devices inside the tailnet. A plugin system is in place to provide extensibility to support different log backend.

### Plugins

Currently, [systemd-journald](https://github.com/rbths/rbths-clients/tree/main/libs/log_grabber/plugins/log_iterators/journald) is the only one supported as log backend, but more is coming as needed.

## Usage

You can download released packages from [here](https://github.com/rbths/rbths-clients/releases). Or follow the installation steps from [here](https://dashboard.rbths.com/setup)

## Building

We use Docker to provide a consistent (cross) build enviroment.

To build the build env:

`sh build-envs.sh`

Then, you can build any code by going into docker. For example, to build for amd64:

`mkdir build-amd64`

`docker run -it -v ./:/app/ amd64-builder /bin/bash`

`cd /app/build-amd64 && cmake .. && make -j8`

## Bugs
Please file any issues about this code or the hosted service on [the issue tracker](https://github.com/rbths/rbths-clients/issues).

## Contributing
PRs welcome! But please file bugs/feature-request. Commit messages should reference issues.

We require Developer Certificate of Origin Signed-off-by lines in commits.

See git log for our commit message style.

## About Us
Rbths-clients are developed by the people at rbths.com

## Open Source License
Please refer to [here](https://github.com/rbths/rbths-clients/tree/main/licenses) for all open source licenses we used.