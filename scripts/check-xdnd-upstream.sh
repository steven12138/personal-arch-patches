#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

latest_niri_tag=$(
  git ls-remote --tags --refs https://github.com/niri-wm/niri.git 'v*' |
    awk -F/ '{print $3}' |
    sort -V |
    tail -n1
)

echo "Checking niri ${latest_niri_tag}"
git clone --quiet --depth 1 --branch "$latest_niri_tag" \
  https://github.com/niri-wm/niri.git "$work/niri"
git -C "$work/niri" apply --check \
  "$repo_root/niri-xwayland-dnd-fix/0001-mark-external-xwayland-client.patch"

smithay_rev=$(
  awk '
    /^\[workspace.dependencies.smithay\]$/ { in_smithay=1; next }
    /^\[/ { in_smithay=0 }
    in_smithay && /^rev = / { gsub(/[" ]/, "", $3); print $3; exit }
  ' "$work/niri/Cargo.toml"
)
test -n "$smithay_rev"
echo "Checking Smithay ${smithay_rev}"
git clone --quiet https://github.com/Smithay/smithay.git "$work/smithay"
git -C "$work/smithay" checkout --quiet "$smithay_rev"
git -C "$work/smithay" apply --check \
  "$repo_root/niri-xwayland-dnd-fix/0002-smithay-external-xwayland-dnd-focus.patch"

echo "Checking xwayland-satellite main"
git clone --quiet --depth 1 https://github.com/Supreeeme/xwayland-satellite.git \
  "$work/xwayland-satellite"
git -C "$work/xwayland-satellite" apply --check \
  "$repo_root/xwayland-satellite-dnd-fix/0001-bidirectional-xdnd.patch"

echo "All XDND patches still apply. Build and run both package test suites before updating commits."
