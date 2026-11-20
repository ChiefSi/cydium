#!/usr/bin/env bash
# Builds a firmware image in a container, and optionally flashes it and opens
# the serial console. Can be run from any directory; the script must live at
# the repository root.
#
# Usage: build.sh [--target NAME] [--test] [--no-build] [--flash] [--console]
#                 [--port DEV] [ENGINE BUILD ARGS...]
#
#   --target NAME  image to build (default: cydium). "cydium" is the project
#                  in src/; any other name is the project in src/apps/NAME/,
#                  e.g. "diagnostic".
#   --test         run the host unit tests (src/test) in the container first;
#                  the build is skipped if they fail
#   --no-build     skip the build and use the existing output in OUT_DIR, e.g.
#                  to reflash or just open the console
#   --flash        after building, flash the image with esptool from .venv
#   --console      after building (and flashing), open the serial console
#                  with pyserial's miniterm from .venv (Ctrl+] to exit)
#   --port DEV     serial port for --flash and --console (default: $PORT, or
#                  /dev/ttyUSB0)
#   -h, --help     show this help
#
# Any other arguments are passed straight to `<engine> build`, e.g.
#   ./build.sh --no-cache
#
# Environment overrides:
#   IDF_VERSION  ESP-IDF git tag (default: the one in Containerfile)
#   IDF_TARGET   chip target (default: esp32)
#   OUT_DIR      output folder (default: <repo>/build/<target>; a relative
#                path is taken from the current directory)
#   PORT         serial port (default: /dev/ttyUSB0)
#   BAUD         flash and console baud rate (default: 115200; some CH340
#                chips on these boards fail at higher rates)
#   CONTAINER_ENGINE  podman or docker (default: podman if installed, else docker)
set -euo pipefail

usage()
{
  sed -n '2,/^set -euo/{/^set -euo/d;s/^# \{0,1\}//;p}' "${BASH_SOURCE[0]}"
}

# Repository root = the directory holding this script, with symlinks resolved.
src=${BASH_SOURCE[0]}
while [[ -L $src ]]; do
  dir=$(cd -P "$(dirname "$src")" && pwd)
  src=$(readlink "$src")
  [[ $src == /* ]] || src="$dir/$src"
done
ROOT=$(cd -P "$(dirname "$src")" && pwd)

# ---------------------------------------------------------------- arguments
TARGET=cydium
BUILD=1
TEST=0
FLASH=0
CONSOLE=0
PORT=${PORT:-/dev/ttyUSB0}
BAUD=${BAUD:-115200}
engine_args=()
while (($#)); do
  case $1 in
    --target)
      [[ $# -ge 2 ]] || { echo "--target needs a name" >&2; exit 2; }
      TARGET=$2
      shift 2
      ;;
    --target=*) TARGET=${1#*=}; shift ;;
    --test) TEST=1; shift ;;
    --no-build) BUILD=0; shift ;;
    --flash) FLASH=1; shift ;;
    --console) CONSOLE=1; shift ;;
    --port)
      [[ $# -ge 2 ]] || { echo "--port needs a device" >&2; exit 2; }
      PORT=$2
      shift 2
      ;;
    --port=*) PORT=${1#*=}; shift ;;
    -h | --help) usage; exit 0 ;;
    *) engine_args+=("$1"); shift ;;
  esac
done

list_targets()
{
  echo cydium
  local d
  for d in "$ROOT"/src/apps/*/; do
    [[ -f $d/CMakeLists.txt ]] && basename "$d"
  done
  return 0
}

if [[ $TARGET == cydium ]]; then
  PROJECT_DIR=$ROOT/src
elif [[ $TARGET =~ ^[A-Za-z0-9_-]+$ && -f $ROOT/src/apps/$TARGET/CMakeLists.txt ]]; then
  PROJECT_DIR=$ROOT/src/apps/$TARGET
else
  echo "unknown target '$TARGET'; available:" $(list_targets) >&2
  exit 2
fi

if ((!BUILD && !TEST)) && ((${#engine_args[@]})); then
  echo "--no-build: unexpected arguments: ${engine_args[*]}" >&2
  exit 2
fi
if ((!BUILD && !TEST && !FLASH && !CONSOLE)); then
  echo "--no-build without --test, --flash or --console has nothing to do" >&2
  exit 2
fi

# Check the host tools up front, so a long build isn't wasted.
PYTHON=$ROOT/.venv/bin/python
if ((FLASH || CONSOLE)); then
  if [[ ! -x $PYTHON ]]; then
    echo "no Python venv at $ROOT/.venv; see Quick start in README.md" >&2
    exit 1
  fi
  if ((FLASH)) && ! "$PYTHON" -c 'import esptool' 2> /dev/null; then
    echo "esptool missing: $ROOT/.venv/bin/pip install esptool" >&2
    exit 1
  fi
  if ((CONSOLE)) && ! "$PYTHON" -c 'import serial.tools.miniterm' 2> /dev/null; then
    echo "pyserial missing: $ROOT/.venv/bin/pip install pyserial" >&2
    exit 1
  fi
fi

ENGINE=${CONTAINER_ENGINE:-}
if ((BUILD || TEST)) && [[ -z $ENGINE ]]; then
  if command -v podman > /dev/null; then
    ENGINE=podman
  elif command -v docker > /dev/null; then
    ENGINE=docker
  else
    echo "neither podman nor docker found; see Dependencies in README.md" >&2
    exit 1
  fi
fi

# ---------------------------------------------------------------- output dir
OUT_DIR=${OUT_DIR:-$ROOT/build/$TARGET}
[[ $OUT_DIR == /* ]] || OUT_DIR="$PWD/$OUT_DIR"
# Normalise (resolve .., . and symlinks) via the parent, created if missing.
out_parent=$(dirname "$OUT_DIR")
out_name=$(basename "$OUT_DIR")
((BUILD)) && mkdir -p -- "$out_parent"
if [[ ! -d $out_parent ]]; then
  echo "no build output at $OUT_DIR; build it first (drop --no-build)" >&2
  exit 1
fi
out_parent=$(cd -P "$out_parent" && pwd)
case $out_name in
  . | .. | /) OUT_DIR=$(cd -P "$out_parent/$out_name" && pwd) ;;
  *) OUT_DIR="${out_parent%/}/$out_name" ;;
esac
# OUT_DIR is deleted and replaced below, so refuse anything that would wipe
# the repository, a parent of it, its sources or the home directory.
out_prefix="${OUT_DIR%/}/"
if [[ ${ROOT}/ == "$out_prefix"* ]]; then
  echo "refusing OUT_DIR=$OUT_DIR: it contains the repository" >&2
  exit 1
fi
if [[ $out_prefix == "$ROOT/src/"* || $out_prefix == "$ROOT/.git/"* ]]; then
  echo "refusing OUT_DIR=$OUT_DIR: it is inside src/ or .git/" >&2
  exit 1
fi
if [[ $OUT_DIR == "$(cd -P ~ && pwd)" ]]; then
  echo "refusing OUT_DIR=$OUT_DIR: it is the home directory" >&2
  exit 1
fi
TMP_DIR="${OUT_DIR}.tmp"

# ---------------------------------------------------------------- engine
engine_common=()
case $ENGINE in
  # Podman: the OCI image format ignores SHELL, so use the Docker format.
  *podman*) engine_common+=(--format docker --layers) ;;
  # Docker: --output and RUN --mount need BuildKit (default since Docker 23).
  *docker*) export DOCKER_BUILDKIT=1 ;;
esac
[[ -n ${IDF_VERSION:-} ]] && engine_common+=(--build-arg "IDF_VERSION=${IDF_VERSION}")
[[ -n ${IDF_TARGET:-} ]] && engine_common+=(--build-arg "IDF_TARGET=${IDF_TARGET}")

# ---------------------------------------------------------------- test
if ((TEST)); then
  echo "Running host unit tests (src/test)"
  # A cached result means src/ is unchanged since the tests last passed.
  "$ENGINE" build --target test "${engine_common[@]}" \
    ${engine_args[@]+"${engine_args[@]}"} -f "$ROOT/Containerfile" "$ROOT"
  echo "Host unit tests passed"
fi

# ---------------------------------------------------------------- build
if ((BUILD)); then
  args=(
    --target artifacts
    --output "type=local,dest=${TMP_DIR}"
    --build-arg "APP=${TARGET}"
    ${engine_common[@]+"${engine_common[@]}"}
  )

  # Version source (see CYD_VERSION_* in Kconfig). The container
  # can't see .git, so describe the checkout here; -dirty marks any change,
  # including untracked files.
  git_describe=unknown
  if git -C "$ROOT" rev-parse --git-dir > /dev/null 2>&1; then
    git_describe=$(git -C "$ROOT" describe --tags --always 2> /dev/null || echo unknown)
    if [[ -n $(git -C "$ROOT" status --porcelain 2> /dev/null) ]]; then
      git_describe+=-dirty
    fi
  fi
  args+=(--build-arg "CYD_GIT_DESCRIBE=${git_describe}")

  echo "Building target '$TARGET' ($PROJECT_DIR), ${git_describe}"
  rm -rf "${TMP_DIR}"
  "$ENGINE" build "${args[@]}" ${engine_args[@]+"${engine_args[@]}"} \
    -f "$ROOT/Containerfile" "$ROOT"

  # Only replace the previous output once the build has succeeded.
  rm -rf "${OUT_DIR}"
  mv "${TMP_DIR}" "${OUT_DIR}"

  # dependencies.lock should be committed; seed it on a project's first build.
  if [[ ! -f $PROJECT_DIR/dependencies.lock && -f ${OUT_DIR}/dependencies.lock ]]; then
    cp "${OUT_DIR}/dependencies.lock" "$PROJECT_DIR/dependencies.lock"
    echo "Created ${PROJECT_DIR#"$ROOT"/}/dependencies.lock - commit it to pin component versions."
  fi

  echo
  echo "Artefacts in ${OUT_DIR}/:"
  ls -l "${OUT_DIR}"
elif ((FLASH)) && [[ ! -f $OUT_DIR/flash_args ]]; then
  echo "no build output at $OUT_DIR; build it first (drop --no-build)" >&2
  exit 1
fi

# ---------------------------------------------------------------- flash / console
if ((FLASH)); then
  echo
  echo "Flashing '$TARGET' to $PORT at $BAUD baud"
  # flash_args uses paths relative to the output folder.
  (cd "$OUT_DIR" && "$PYTHON" -m esptool --chip "${IDF_TARGET:-esp32}" -p "$PORT" -b "$BAUD" \
    --before default-reset --after hard-reset write-flash @flash_args)
fi

if ((CONSOLE)); then
  echo
  echo "Serial console on $PORT at $BAUD baud (Ctrl+] to exit)"
  # Hold DTR/RTS inactive so opening the port doesn't keep the ESP32 in reset.
  exec "$PYTHON" -m serial.tools.miniterm --dtr 0 --rts 0 "$PORT" "$BAUD"
fi
