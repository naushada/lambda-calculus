#!/bin/sh
# Usage: tests/run.sh path/to/lc
set -u
LC="${1:-build/lc}"
dir=$(dirname "$0")
fail=0

check() {                       # check <label> <expected> <actual>
    if [ "$2" = "$3" ]; then
        echo "ok"
    else
        echo "FAIL"
        printf '  expected: %s\n  actual:   %s\n' "$2" "$3"
        fail=1
    fi
}

printf 'parse vectors:    '
out=$("$LC" -p "$dir/cases.lc" 2>/dev/null)
check "parse" "$(cat "$dir/expected.txt")" "$out"

printf 'beta reduction:   '
out=$("$LC" -s 200 "$dir/eval.lc" 2>/dev/null)
check "eval" "$(cat "$dir/eval-expected.txt")" "$out"

# Every line but the last is malformed; the last must still parse, proving
# error recovery resynchronises at the newline instead of cascading.
printf 'definitions:      '
out=$("$LC" -s 200 "$dir/defs.lc" 2>/dev/null)
check "defs" "$(cat "$dir/defs-expected.txt")" "$out"

printf 'error recovery:   '
out=$("$LC" -p "$dir/errors.lc" 2>/dev/null); rc=$?
n=$("$LC" -p "$dir/errors.lc" 2>&1 >/dev/null | grep -c 'line ')
if [ "$rc" -eq 1 ] && [ "$out" = "(f x)" ] && [ "$n" -ge 6 ]; then
    echo "ok ($n diagnostics, exit 1, recovered to parse the good line)"
else
    echo "FAIL (exit=$rc diagnostics=$n out='$out')"
    fail=1
fi

# Omega has no normal form: the step limit must stop it and the exit status
# must report it, rather than the process hanging.
printf 'divergence:       '
out=$(printf '(\316\273x.(x x) \316\273x.(x x))\n' | "$LC" -s 25 2>/dev/null); rc=$?
case "$out" in
    *"no normal form after 25 steps"*)
        [ "$rc" -eq 1 ] && echo "ok (stopped at the limit, exit 1)" \
                        || { echo "FAIL (exit=$rc, expected 1)"; fail=1; } ;;
    *) echo "FAIL (out='$out')"; fail=1 ;;
esac

# The strategies genuinely differ: normal order reaches y because it never
# evaluates the unused divergent argument; applicative order does and hangs.
printf 'strategy split:   '
term='(\316\273x.y (\316\273x.(x x) \316\273x.(x x)))\n'
n_out=$(printf "$term" | "$LC"    -s 25 2>/dev/null)
a_out=$(printf "$term" | "$LC" -a -s 25 2>/dev/null)
if [ "$n_out" = "y" ] && [ "${a_out#*no normal form}" != "$a_out" ]; then
    echo "ok (normal order -> y; applicative order diverges)"
else
    echo "FAIL (normal='$n_out' applicative='$a_out')"
    fail=1
fi

[ "$fail" -eq 0 ] && echo "PASS" || echo "FAILURES"
exit $fail
