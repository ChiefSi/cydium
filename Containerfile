# Build environment for cydium (Podman or Docker). Use ./build.sh rather than
# calling this directly. Stages:
#   deps      - OS packages, ESP-IDF source and tools (cached between builds)
#   build     - compiles one image (APP) from src/, a read-only bind mount
#   artifacts - only the build outputs, exported with --output

ARG UBUNTU_IMAGE=docker.io/library/ubuntu:24.04
ARG IDF_VERSION=v6.1.1
ARG IDF_TARGET=esp32

########################################################################
FROM ${UBUNTU_IMAGE} AS deps
ARG IDF_VERSION
ARG IDF_TARGET

ENV DEBIAN_FRONTEND=noninteractive \
    IDF_PATH=/opt/esp-idf \
    IDF_TOOLS_PATH=/opt/espressif \
    IDF_COMPONENT_CACHE_PATH=/root/.cache/idf-components \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    LANG=C.UTF-8

# Podman needs --format docker for this (build.sh sets it); OCI ignores SHELL.
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Keep downloaded .debs so the apt cache mount below is actually useful.
RUN rm -f /etc/apt/apt.conf.d/docker-clean \
 && echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' \
      > /etc/apt/apt.conf.d/keep-cache

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update \
 && apt-get install -y --no-install-recommends \
      bison ca-certificates ccache cmake dfu-util flex git gperf \
      libffi-dev libssl-dev libusb-1.0-0 ninja-build \
      python3 python3-pip python3-venv wget xz-utils

RUN git clone --depth 1 --branch "${IDF_VERSION}" --recursive --shallow-submodules \
      https://github.com/espressif/esp-idf.git "${IDF_PATH}"

# Tool archives (dist/) and pip wheels live in cache mounts, so a rebuild of
# this layer (e.g. after changing IDF_VERSION) doesn't re-download them all.
# The extracted tools and Python venv end up in the image layer itself.
RUN --mount=type=cache,target=/opt/espressif/dist \
    --mount=type=cache,target=/root/.cache/pip \
    "${IDF_PATH}/install.sh" "${IDF_TARGET}"

########################################################################
FROM deps AS build
ARG IDF_TARGET
# Image to build: "cydium" is the project in src/, any other name is the
# project in src/apps/<name>/. build.sh checks the name before building.
ARG APP=cydium

# Only src/ is bind-mounted (bind mounts are read-only by default). The
# ignore file limits the build context to src/, so the layer cache reruns this step exactly when the
# sources change. idf.py writes sdkconfig, dependencies.lock and
# managed_components/ into the project dir, so it builds from a scratch copy.
RUN --mount=type=bind,source=src,target=/src \
    --mount=type=cache,target=/root/.cache/idf-components \
    mkdir -p /work/src \
 && cp -a /src/. /work/src/ \
 && . "${IDF_PATH}/export.sh" > /dev/null \
 && if [[ $APP == cydium ]]; then proj=/work/src; else proj=/work/src/apps/$APP; fi \
 && cd "$proj" \
 && idf.py -B /work/build set-target "${IDF_TARGET}" \
 && idf.py -B /work/build build \
 && mkdir -p /out \
 && cd /work/build \
 && cp -- *.bin *.elf *.map flash_args flasher_args.json /out/ \
 && cp --parents -- bootloader/bootloader.bin partition_table/partition-table.bin /out/ \
 && cp -- "$proj/sdkconfig" "$proj/dependencies.lock" /out/

########################################################################
FROM scratch AS artifacts
COPY --from=build /out/ /
