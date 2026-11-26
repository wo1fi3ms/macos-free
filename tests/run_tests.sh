#!/bin/sh
# free için duman testleri / Smoke tests for free
# Kullanım / Usage: tests/run_tests.sh ./free
set -u
BIN=${1:-./free}
fail=0
pass=0

ok()   { pass=$((pass + 1)); }
bad()  { echo "FAIL: $*"; fail=$((fail + 1)); }

# Komut başarıyla çıkmalı / Command must exit 0
expect_ok() {
    if "$BIN" "$@" >/dev/null 2>&1; then ok; else bad "free $* exit != 0"; fi
}
# Komut hata vermeli / Command must exit non-zero
expect_err() {
    if "$BIN" "$@" >/dev/null 2>&1; then bad "free $* should fail"; else ok; fi
}

for args in "" "-b" "-k" "-m" "-g" "--kilo" "--mega" "--giga" "--tera" "--peta" \
            "--tebi" "--pebi" "-h" "-h --si" "-w" "-t" "-L" "-d" "-c 2 -s 0.1" \
            "--help" "-V"; do
    # shellcheck disable=SC2086
    expect_ok $args
done

for args in "-l" "-v" "-s 0" "-s abc" "-c 0" "-c -1" "--bogus" "extra"; do
    # shellcheck disable=SC2086
    expect_err $args
done

# Satır sayısı / Line counts
[ "$("$BIN" | wc -l | tr -d ' ')" = 3 ] && ok || bad "default output should have 3 lines"
[ "$("$BIN" -t | wc -l | tr -d ' ')" = 4 ] && ok || bad "-t output should have 4 lines"
[ "$("$BIN" -L | wc -l | tr -d ' ')" = 1 ] && ok || bad "-L output should have 1 line"
[ "$("$BIN" -c 3 -s 0.1 | grep -c '^Mem:')" = 3 ] && ok || bad "-c 3 should print 3 tables"

# Bayt modunda used + free + buff/cache = total ve available <= total
# In bytes mode used + free + buff/cache = total and available <= total
"$BIN" -b | awk '/^Mem:/ {
    if ($3 + $4 + $6 != $2) { print "FAIL: used+free+buff/cache != total"; exit 1 }
    if ($7 > $2)            { print "FAIL: available > total"; exit 1 }
}' && ok || bad "Mem row invariants"

# total sysctl hw.memsize ile aynı olmalı / total must equal sysctl hw.memsize
memsize=$(sysctl -n hw.memsize)
total=$("$BIN" -b | awk '/^Mem:/ {print $2}')
[ "$memsize" = "$total" ] && ok || bad "total ($total) != hw.memsize ($memsize)"

# -k varsayılanla aynı birimde olmalı / -k must use the same unit as the default
t1=$("$BIN" | awk '/^Mem:/ {print $2}')
t2=$("$BIN" -k | awk '/^Mem:/ {print $2}')
[ "$t1" = "$t2" ] && ok || bad "-k total differs from default"

# --si ile -k 1000'e bölmeli / -k with --si must divide by 1000
t3=$("$BIN" -k --si | awk '/^Mem:/ {print $2}')
[ "$t3" = "$((memsize / 1000))" ] && ok || bad "--si -k total ($t3) != memsize/1000"

echo "passed: $pass, failed: $fail"
[ "$fail" = 0 ]
