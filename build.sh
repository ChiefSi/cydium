#!/usr/bin/env bash
# Builds the firmware in a container and copies the artefacts to ./build.
#
# Environment overrides:
#   IDF_VERSION  ESP-IDF git tag (default: the one in Containerfile)
#   IDF_TARGET   chip target (default: esp32)
#   OUT_DIR      output folder (default: build)
#   CONTAINER_ENGINE  podman or docker (default: podman if installed, else docker)
# Extra arguments are passed straight to `<engine> build`, e.g.
#   ./build.sh --no-cache
set -euo pipefail

cd "$(dirname "$(readlink -f "$0")")"

ENGINE=${CONTAINER_ENGINE:-}
if [[ -z $ENGINE ]]; then
  if command -v podman > /dev/null; then
    ENGINE=podman
  elif command -v docker > /dev/null; then
    ENGINE=docker
  else
    echo "neither podman nor docker found; see Dependencies in README.md" >&2
    exit 1
  fi
fi

OUT_DIR=${OUT_DIR:-build}
TMP_DIR="${OUT_DIR}.tmp"

args=(
  --target artifacts
  --output "type=local,dest=${TMP_DIR}"
)
case $ENGINE in
  # Podman: the OCI image format ignores SHELL, so use the Docker format.
  *podman*) args+=(--format docker --layers) ;;
  # Docker: --output and RUN --mount need BuildKit (default since Docker 23).
  *docker*) export DOCKER_BUILDKIT=1 ;;
esac
[[ -n ${IDF_VERSION:-} ]] && args+=(--build-arg "IDF_VERSION=${IDF_VERSION}")
[[ -n ${IDF_TARGET:-} ]] && args+=(--build-arg "IDF_TARGET=${IDF_TARGET}")

rm -rf "${TMP_DIR}"
"$ENGINE" build "${args[@]}" "$@" -f Containerfile .

# Only replace the previous output once the build has succeeded.
rm -rf "${OUT_DIR}"
mv "${TMP_DIR}" "${OUT_DIR}"

# The plan wants dependencies.lock committed; seed it on the first build.
if [[ ! -f src/dependencies.lock && -f ${OUT_DIR}/dependencies.lock ]]; then
  cp "${OUT_DIR}/dependencies.lock" src/dependencies.lock
  echo "Created src/dependencies.lock - commit it to pin component versions."
fi

echo
echo "Artefacts in ${OUT_DIR}/:"
ls -l "${OUT_DIR}"
