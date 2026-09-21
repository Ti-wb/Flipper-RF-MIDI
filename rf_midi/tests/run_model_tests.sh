#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_binary=$(mktemp "${TMPDIR:-/tmp}/rf-midi-model.XXXXXX")
trap 'rm -f "$test_binary"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
    -I"$source_dir" "$source_dir/rf_midi_model.c" "$source_dir/tests/test_model.c" \
    -o "$test_binary"
"$test_binary"
