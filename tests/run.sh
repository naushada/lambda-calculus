#!/bin/sh
# Usage: tests/run.sh path/to/lc
set -u
LC="${1:-build/lc}"
dir=$(dirname "$0")
fail=0

printf 'good cases: '
if out=$("$LC" "$dir/cases.lc" 2>/dev/null) &&
   [ "$out" = "$(cat "$dir/expected.txt")" ]; then
    echo "ok ($(wc -l < "$dir/expected.txt" | tr -d ' ') vectors)"
else
    echo "FAIL"
    diff -u "$dir/expected.txt" - <<EOT || true
$out
EOT
    fail=1
fi

# Every line but the last is malformed; the last must still parse, proving
# error recovery resynchronises at the newline instead of cascading.
printf 'error recovery: '
out=$("$LC" "$dir/errors.lc" 2>/dev/null); rc=$?
n=$("$LC" "$dir/errors.lc" 2>&1 >/dev/null | grep -c 'line ')
if [ "$rc" -eq 1 ] && [ "$out" = "(f x)" ] && [ "$n" -ge 6 ]; then
    echo "ok ($n diagnostics, exit 1, recovered to parse the good line)"
else
    echo "FAIL (exit=$rc diagnostics=$n out='$out')"
    fail=1
fi

[ "$fail" -eq 0 ] && echo "PASS" || echo "FAILURES"
exit $fail
