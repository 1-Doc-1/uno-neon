#!/usr/bin/env bash
# Vérification Linux du serveur (GCC + Clang), sans sudo : même recette que
# .github/workflows/ci.yml (job server-linux), à lancer dans WSL avant tout push qui touche server/.
#
# Copie l'arbre de travail vers un dossier natif WSL (compiler directement sur /mnt/c est très lent),
# puis pour gcc et pour clang : configure, build et ctest (preset debug-asan), puis clang-format
# et clang-tidy (une seule fois, avec le build clang).
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

echo "==> Synchronisation de l'arbre de travail vers $DEST_DIR"
mkdir -p "$DEST_DIR"
rsync -a --delete \
  --exclude 'build/' \
  --exclude 'node_modules/' \
  --exclude '.git/' \
  --exclude '.vcpkg-cache/' \
  "$SRC_DIR/" "$DEST_DIR/"

cd "$DEST_DIR/server"

run_for_compiler() {
  local name="$1" cc="$2" cxx="$3"
  echo
  echo "=== $name : configure + build + tests (debug-asan) ==="
  rm -rf build/debug-asan
  CC="$cc" CXX="$cxx" cmake --preset debug-asan
  CC="$cc" CXX="$cxx" cmake --build --preset debug-asan
  ctest --preset debug-asan
}

run_for_compiler gcc gcc-14 g++-14
run_for_compiler clang clang-20 clang++-20

echo
echo "=== clang-format (dry-run) ==="
git ls-files '*.cpp' '*.hpp' '*.cpp.in' | xargs clang-format-23 --dry-run --Werror

echo
echo "=== clang-tidy ==="
git ls-files '*.cpp' | xargs clang-tidy-23 -p build/debug-asan --quiet

echo
echo "OK : gcc-14 et clang-20 compilent et testent en debug-asan, clang-format et clang-tidy propres."
