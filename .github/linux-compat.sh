#!/usr/bin/env bash
# Checks that every Linux plugin binary under the given folders loads on Debian 12
# (Raspberry Pi OS Bookworm, Ubuntu 22.04 and newer have the same or newer): it may
# need at most glibc 2.36 and GCC 12's libstdc++ (GLIBCXX 3.4.30, CXXABI 1.3.13).
#
# Usage: .github/linux-compat.sh <folder>...
set -euo pipefail
max_glibc=2.36 max_glibcxx=3.4.30 max_cxxabi=1.3.13
newer() { [[ "$1" != "$2" && "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -n 1)" == "$1" ]]; }

mapfile -d '' libs < <(find "$@" -type f -name '*.so' -print0)
(( ${#libs[@]} )) || { echo "::error::No plugin binaries (*.so) under $*"; exit 1; }
bad=0
for lib in "${libs[@]}"; do
  syms=$(objdump -T "$lib")
  need() { { grep -o "$1_[0-9.]*" <<<"$syms" || true; } | sed "s/^$1_//" | sort -uV | tail -n 1; }
  g=$(need GLIBC); x=$(need GLIBCXX); a=$(need CXXABI)
  echo "${lib}: glibc ${g:-none}, GLIBCXX ${x:-none}, CXXABI ${a:-none}"
  if [[ -n "$g" ]] && newer "$g" "$max_glibc"; then echo "::error file=$lib::needs glibc $g (Debian 12 has $max_glibc)"; bad=1; fi
  if [[ -n "$x" ]] && newer "$x" "$max_glibcxx"; then echo "::error file=$lib::needs GLIBCXX $x (Debian 12 has $max_glibcxx)"; bad=1; fi
  if [[ -n "$a" ]] && newer "$a" "$max_cxxabi"; then echo "::error file=$lib::needs CXXABI $a (Debian 12 has $max_cxxabi)"; bad=1; fi
done
exit "$bad"
