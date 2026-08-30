#!/bin/sh
# Differential test: the flex/bison and hand-written implementations must
# agree on the language.
#
#   stdout    compared byte for byte -- the results must be identical
#   stderr    compared structurally: the same input lines must be flagged,
#             the same number of times.  The wording deliberately differs.
#             Bison emits a bare "syntax error"; the recursive-descent parser
#             knows what it was expecting and says so ("expected ')' to close
#             the application").  Better diagnostics are one of the genuine
#             arguments for hand-writing a parser, so the test pins the
#             agreement without forcing the C version to get worse.
#   exit      must match
#
# Usage: tests/compare.sh build/lc cpp/build/lc
set -u
A="${1:-build/lc}"          # flex/bison, C
B="${2:-cpp/build/lc}"      # hand-written, C++
dir=$(dirname "$0")
fail=0

# The lines a run flagged, e.g. "line 3" repeated per diagnostic.
lines_flagged() { sed -n 's/^\(line [0-9]*\):.*/\1/p'; }

compare() {                 # compare <label> <flags> <file>
    label="$1"; flags="$2"; file="$3"

    a_out=$("$A" $flags "$file" 2>/tmp/lc-a.err); a_rc=$?
    b_out=$("$B" $flags "$file" 2>/tmp/lc-b.err); b_rc=$?

    if [ "$a_out" != "$b_out" ]; then
        printf '  DIFFER (stdout): %s\n' "$label"
        printf '%s\n' "$a_out" > /tmp/lc-a.out
        printf '%s\n' "$b_out" > /tmp/lc-b.out
        diff -u /tmp/lc-a.out /tmp/lc-b.out | head -20
        fail=1; return
    fi
    if [ "$a_rc" != "$b_rc" ]; then
        printf '  DIFFER (exit): %s -- C=%s C++=%s\n' "$label" "$a_rc" "$b_rc"
        fail=1; return
    fi
    if ! lines_flagged < /tmp/lc-a.err > /tmp/lc-a.lines ||
       ! lines_flagged < /tmp/lc-b.err > /tmp/lc-b.lines; then :; fi
    if ! diff -q /tmp/lc-a.lines /tmp/lc-b.lines >/dev/null; then
        printf '  DIFFER (diagnosed lines): %s\n' "$label"
        diff -u /tmp/lc-a.lines /tmp/lc-b.lines | head -20
        fail=1; return
    fi
    printf '  agree: %s\n' "$label"
}

# Byte-level edge cases the two scanners reach by different mechanisms:
# flex byte-classes versus one character of lookahead.
printf 'x\316\273y\n\316\274\n\317\200r2\n\316\273\316\274.\316\274\n\316\n\316 x\nx\316\ny\nK=\316\273x.\316\273y.x\nx=y\nx+y\nx1\n1x\n' \
    > /tmp/lc-edge.lc

for file in "$dir"/cases.lc "$dir"/eval.lc "$dir"/defs.lc "$dir"/eta.lc \
            "$dir"/errors.lc /tmp/lc-edge.lc; do
    for flags in "-p" "-s 200" "-a -s 200" "-t -s 40" "-e -s 200" \
                 "-e -a -s 200"; do
        compare "$(basename "$file") [$flags]" "$flags" "$file"
    done
done

[ "$fail" -eq 0 ] && echo "IMPLEMENTATIONS AGREE" || echo "DIVERGENCE"
exit $fail
