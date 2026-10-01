#!/usr/bin/env bash
# Vérification Linux du serveur (GCC + Clang), sans sudo : même recette que
# .github/workflows/ci.yml (job server-linux), à lancer dans WSL avant tout push qui touche server/.
#
# Copie l'arbre de travail vers un dossier natif WSL (compiler directement sur /mnt/c est très lent),
# puis pour gcc et pour clang : configure+build et ctest (preset debug-asan), puis clang-format
# et clang-tidy (avec le build clang). Silencieux : une ligne OK/ÉCHEC par étape, détail dans
# ~/uno-neon-linux/linux-check.log (chemin affiché en cas d'échec).
#
# Usage, depuis PowerShell, à la racine du repo :
#   wsl -d Ubuntu-24.04 -- bash scripts/linux-check.sh
#
# Si un outil manque, ce script échoue avec un message qui indique de lancer linux-setup.sh d'abord.

set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="$HOME/uno-neon-linux"
VCPKG_ROOT="$HOME/vcpkg"

fail() {
  echo "ERREUR : $1" >&2
  echo "Lance d'abord, dans WSL : bash scripts/linux-setup.sh (demande le mot de passe sudo)" >&2
  exit 1
}

require_tool() {
  command -v "$1" >/dev/null 2>&1 || fail "outil manquant : $1"
}

for tool in gcc-14 g++-14 clang-20 clang++-20 clang-format-23 clang-tidy-23 cmake ninja rsync git; do
  require_tool "$tool"
done
[ -x "$VCPKG_ROOT/vcpkg" ] || fail "vcpkg absent ou non bootstrappé dans $VCPKG_ROOT"

export VCPKG_ROOT
# Même nombre de parties simulées que le CI des PR.
export UNO_SIMULATION_GAMES=100

# Une compilation ASan peut prendre 1 à 2 Go : le parallélisme est borné par la mémoire disponible
# (au plus un job pour 2 Go) autant que par les cœurs, sinon Ninja (un job par cœur) sature la machine.
available_gb=$(awk '/^MemAvailable:/ { printf "%d", $2 / 1048576 }' /proc/meminfo)
jobs=$(( available_gb / 2 ))
cores=$(nproc)
[ "$jobs" -gt "$cores" ] && jobs=$cores
[ "$jobs" -lt 1 ] && jobs=1
export CMAKE_BUILD_PARALLEL_LEVEL="$jobs"
export VCPKG_MAX_CONCURRENCY="$jobs"
echo "Parallélisme : $jobs (mémoire disponible : ${available_gb} Go, cœurs : $cores)" >> "$HOME/uno-neon-linux-parallelism.log"

LOG="$DEST_DIR/linux-check.log"
mkdir -p "$DEST_DIR"
: > "$LOG"

# Chaque étape écrit son détail dans le log ; l'écran n'affiche qu'une ligne OK / ÉCHEC par étape.
step() {
  local label="$1"
  shift
  echo "=== $label ===" >> "$LOG"
  if "$@" >> "$LOG" 2>&1; then
    echo "OK     $label"
  else
    echo "ÉCHEC  $label"
    echo "Log complet : $LOG"
    exit 1
  fi
}

sync_tree() {
  rsync -a --delete \
    --exclude 'build/' \
    --exclude 'node_modules/' \
    --exclude '.git/' \
    --exclude '.vcpkg-cache/' \
    --exclude 'linux-check.log' \
    "$SRC_DIR/" "$DEST_DIR/"
}

step "synchronisation vers $DEST_DIR" sync_tree

cd "$DEST_DIR/server"

configure_and_build() {
  rm -rf build/debug-asan
  CC="$1" CXX="$2" cmake --preset debug-asan
  CC="$1" CXX="$2" cmake --build --preset debug-asan
}

run_for_compiler() {
  local name="$1" cc="$2" cxx="$3"
  step "$name : configure + build (debug-asan)" configure_and_build "$cc" "$cxx"
  step "$name : tests (debug-asan)" ctest --preset debug-asan --timeout 120
}

# $DEST_DIR has no .git (excluded from the rsync), so the file list comes from the original
# repo; paths are the same relative to server/ in both trees. --others --exclude-standard adds
# not-yet-`git add`ed files too, so a new .cpp isn't missed locally only for the CI to catch it.
check_format() {
  git -C "$SRC_DIR" ls-files --cached --others --exclude-standard \
    'server/*.cpp' 'server/*.hpp' 'server/*.cpp.in' \
    | sed 's|^server/||' | xargs clang-format-23 --dry-run --Werror
}

check_tidy() {
  git -C "$SRC_DIR" ls-files --cached --others --exclude-standard 'server/*.cpp' \
    | sed 's|^server/||' | xargs clang-tidy-23 -p build/debug-asan --quiet
}

run_for_compiler gcc gcc-14 g++-14
run_for_compiler clang clang-20 clang++-20
step "clang-format" check_format
step "clang-tidy" check_tidy

echo "Tout est OK (log : $LOG)"
