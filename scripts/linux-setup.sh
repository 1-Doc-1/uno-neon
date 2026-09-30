#!/usr/bin/env bash
# Installation unique des outils Linux pour UNO Néon, dans WSL (Ubuntu 24.04).
# Mêmes sources et versions que .github/workflows/ci.yml (job server-linux) : GCC 14 et Clang 20
# (dépôts officiels Ubuntu 24.04 "noble"), clang-format/clang-tidy 23 (apt.llvm.org), vcpkg à la
# baseline de server/vcpkg.json.
#
# Utilise sudo : à lancer soi-même dans un terminal WSL, jamais depuis un script automatisé.
# Idempotent : peut être relancé sans risque (sauts si déjà installé).
#
# Usage (depuis WSL Ubuntu-24.04, n'importe quel dossier) :
#   bash scripts/linux-setup.sh
# ou depuis PowerShell :
#   wsl -d Ubuntu-24.04 -- bash /mnt/c/dev/uno-neon/scripts/linux-setup.sh

set -euo pipefail

VCPKG_COMMIT="4cb050be2cfa7a947cdd2dd1a70e24b17774c979" # server/vcpkg.json, builtin-baseline
LLVM_KEY_FINGERPRINT="6084F3CF814B57C1CF12EFD515CF4D18AF4F7421"
VCPKG_ROOT="$HOME/vcpkg"

echo "==> Mise à jour des index apt"
sudo apt-get update

echo "==> Paquets de base (GCC 14, Clang 20, autotools, ninja, cmake, rsync — dépôts officiels Ubuntu 24.04)"
sudo apt-get install -y --no-install-recommends \
  build-essential ninja-build cmake g++-14 \
  autoconf autoconf-archive automake libtool pkg-config \
  clang-20 libclang-rt-20-dev \
  curl gnupg ca-certificates rsync git zip unzip

echo "==> clang-format et clang-tidy 23 (apt.llvm.org, comme le CI — version des postes de l'équipe)"
if ! command -v clang-format-23 >/dev/null 2>&1 || ! command -v clang-tidy-23 >/dev/null 2>&1; then
  tmp_key="$(mktemp)"
  curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key -o "$tmp_key"
  actual_fingerprint=$(gpg --show-keys --with-colons "$tmp_key" | awk -F: '$1 == "fpr" { print $10; exit }')
  if [ "$actual_fingerprint" != "$LLVM_KEY_FINGERPRINT" ]; then
    echo "Empreinte de clé apt.llvm.org inattendue : $actual_fingerprint" >&2
    rm -f "$tmp_key"
    exit 1
  fi
  sudo install -D -m 0644 "$tmp_key" /etc/apt/keyrings/llvm.asc
  rm -f "$tmp_key"
  echo "deb [signed-by=/etc/apt/keyrings/llvm.asc] https://apt.llvm.org/noble/ llvm-toolchain-noble-23 main" \
    | sudo tee /etc/apt/sources.list.d/llvm-23.list
  sudo apt-get update
  sudo apt-get install -y clang-format-23 clang-tidy-23
else
  echo "Déjà présents, rien à faire."
fi

echo "==> Vérification des versions"
g++-14 --version | head -1
clang++-20 --version | head -1
clang-format-23 --version | grep -E 'clang-format version 23\.'
clang-tidy-23 --version | grep -E 'LLVM version 23\.'

echo "==> vcpkg dans $VCPKG_ROOT (commit $VCPKG_COMMIT)"
if [ -d "$VCPKG_ROOT/.git" ]; then
  git -C "$VCPKG_ROOT" fetch origin
else
  git clone --filter=blob:none https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
fi
git -C "$VCPKG_ROOT" checkout "$VCPKG_COMMIT"
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics

echo
echo "Terminé. Vérifie avec :"
echo "  wsl -d Ubuntu-24.04 -- bash scripts/linux-check.sh"
echo "(depuis PowerShell, à la racine du repo C:\\dev\\uno-neon)"
