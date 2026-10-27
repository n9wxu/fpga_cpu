#!/bin/sh
# Put an IWAD in benchmarks/doom/wad/ for the Doom benchmark.
#
# If you have the shareware DOOM1.WAD (v1.9), copy it to wad/DOOM1.WAD
# yourself: it is used first, and its demo1 is the classic timedemo.
# Otherwise this fetches Freedoom: Phase 1 (BSD licensed) from its GitHub
# release and installs freedoom1.wad as wad/DOOM.WAD (the file service
# takes 8.3 names).
set -e
cd "$(dirname "$0")"
mkdir -p wad
if [ -f wad/DOOM1.WAD ]; then
  sum=$(sha1sum wad/DOOM1.WAD | cut -d' ' -f1)
  [ "$sum" = 5b2e249b9c5133ec987b3ea77596381dc0d6bc1d ] && echo "wad/DOOM1.WAD: shareware v1.9" \
    || echo "wad/DOOM1.WAD: not the v1.9 shareware file (sha1 $sum); used anyway"
  exit 0
fi
if [ -f wad/DOOM.WAD ]; then
  echo "wad/DOOM.WAD already there"
  exit 0
fi
VER=0.13.0
URL=https://github.com/freedoom/freedoom/releases/download/v$VER/freedoom-$VER.zip
SHA256=7323bcc168c5a45ff10749b339960e98314740a734c30d4b9f3337001f9e703d
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
curl -sSfL -o "$tmp/fd.zip" "$URL"
python3 -m zipfile -e "$tmp/fd.zip" "$tmp"
f="$tmp/freedoom-$VER/freedoom1.wad"
echo "$SHA256  $f" | sha256sum -c -
cp "$f" wad/DOOM.WAD
cp "$tmp/freedoom-$VER/COPYING.txt" wad/FREEDOOM-COPYING.txt
echo "wad/DOOM.WAD: Freedoom: Phase 1 $VER"
