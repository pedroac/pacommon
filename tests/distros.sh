#!/bin/sh
# Builds pacommon and runs its tests in a container of each of several distributions, so that
# nothing depends on Arch Linux's layout, libc or compiler. Needs podman or docker.
#   tests/distros.sh [image ...]      (default: the list below)
# Alpine uses musl, which AddressSanitizer does not support: it only runs `make test`.
set -u
cd "$(dirname "$0")/.." || exit 2

engine=$(command -v podman || command -v docker) || { echo "distros.sh: need podman or docker" >&2; exit 2; }

default_images="docker.io/library/archlinux docker.io/library/debian:stable docker.io/library/ubuntu:latest
docker.io/library/fedora:latest docker.io/library/alpine:latest docker.io/opensuse/tumbleweed"

# The command that installs a compiler and make, and the make targets that are run.
recipe() {
    case "$1" in
        */archlinux*)      echo "pacman -Sy --noconfirm gcc make;test sanitize" ;;
        */debian*|*/ubuntu*) echo "apt-get update -qq && apt-get install -y -qq gcc make libc6-dev;test sanitize" ;;
        */fedora*)         echo "dnf install -y -q gcc make libasan libubsan;test sanitize" ;;
        */alpine*)         echo "apk add --no-cache build-base;test" ;;
        */opensuse*)       echo "zypper -q install -y gcc make;test" ;;
        *)                 return 1 ;;
    esac
}

failed=
for image in ${*:-$default_images}; do
    echo "=== $image"
    r=$(recipe "$image") || { echo "--- $image: no recipe, skipped" >&2; failed="$failed $image"; continue; }
    install=${r%;*}
    targets=${r##*;}
    # The source is copied, so that a build inside the container leaves nothing behind.
    if $engine run --rm -v "$PWD":/src:ro "$image" sh -c \
        "$install && cp -r /src /work && cd /work && make clean && make $targets"; then
        echo "--- $image: ok"
    else
        echo "--- $image: FAILED" >&2
        failed="$failed $image"
    fi
done
[ -z "$failed" ] || { echo "failed on:$failed" >&2; exit 1; }
