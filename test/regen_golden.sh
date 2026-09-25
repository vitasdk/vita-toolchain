#!/bin/sh
# Regenerates the golden VELF fixtures used by test_elf_create.py's layout
# regression check (#47). Run this after an intentional change to VELF
# layout/alignment, then review the resulting diff of fixtures/*.velf.
#
# Usage: test/regen_golden.sh <path-to-vita-elf-create>

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <path-to-vita-elf-create>" >&2
    exit 1
fi

ELF_CREATE="$1"
FIXTURES_DIR="$(cd "$(dirname "$0")/fixtures" && pwd)"

"$ELF_CREATE" "$FIXTURES_DIR/sample.elf" "$FIXTURES_DIR/sample.velf"
"$ELF_CREATE" -n "$FIXTURES_DIR/sample_exidx.elf" "$FIXTURES_DIR/sample_exidx.velf"

echo "Regenerated $FIXTURES_DIR/sample.velf and sample_exidx.velf — review with 'git diff --stat' and the section-table output from a failing test run before committing."
