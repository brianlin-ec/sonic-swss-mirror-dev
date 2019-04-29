## Introduction
Unit test environment for SONiC sonic-swss/orchagent

## Getting Started
```
git clone https://github.com/ezio-chen/sonic-swss-dev
cd sonic-swss-dev/

sudo bash -x install-pkg.sh -g

cd ..
mkdir <build-dir> && cd <build-dir>
bash -x <source-dir>/build.sh
source packages/.env


# Run the tests
## Start the redis-server with UNIX socket at the first time
bash -x <build-dir>/redis/start_redis.sh

<build-dir>/tests.out

## Stop the redis-server after you don't need it.
bash -x redis/stop_redis.sh
