#!/bin/sh
# Usage: tests/tokens.sh path/to/lc-tokens
# Checks the scanner's token stream against the vectors documented in
# docs/CPP_SCANNER_DESIGN.md §8.  C++ only -- the flex build has no dumper.
set -u
TOK="${1:-cpp/build/lc-tokens}"
dir=$(dirname "$0")
fail=0

printf 'token vectors:    '
out=$("$TOK" "$dir/tokens.lc" 2>/dev/null)
if [ "$out" = "$(cat "$dir/tokens-expected.txt")" ]; then
    echo "ok ($(grep -vc '^#' "$dir/tokens.lc" | tr -d ' ') lines)"
else
    echo "FAIL"
    printf '%s\n' "$out" > /tmp/lc-tok.out
    diff -u "$dir/tokens-expected.txt" /tmp/lc-tok.out | head -20
    fail=1
fi

# Bytes that cannot live in a committed text file: a lone 0xCE is truncated
# UTF-8, and must be reported rather than absorbed into a name.
printf 'truncated utf-8:  '
out=$(printf 'x\316\ny\n' | "$TOK" 2>/dev/null); rc=$?
err=$(printf 'x\316\ny\n' | "$TOK" 2>&1 >/dev/null)
case "$err" in
    *"truncated UTF-8"*)
        [ "$rc" -eq 1 ] && echo "ok (reported, exit 1)" \
                        || { echo "FAIL (exit=$rc)"; fail=1; } ;;
    *) echo "FAIL (not reported: '$err')"; fail=1 ;;
esac

[ "$fail" -eq 0 ] && echo "PASS" || echo "FAILURES"
exit $fail
