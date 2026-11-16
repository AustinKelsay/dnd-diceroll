#!/usr/bin/env bash
# usage: sweepset.sh "NAME:REV:strategies ..." ...  Runs every listed measurement inside ONE machine-lock hold (run from the checkout).
set -u
mkdir -p .local/logs
inner() {
  for spec in "$@"; do
    IFS=: read -r name rev strategies <<<"$spec"
    for s in ${strategies//,/ }; do
      out=.local/balance/$name-$s; rm -rf "$out"
      DND_BALANCE_REVISION=$rev DND_BALANCE_REWARD=$s DND_BALANCE_OUT=$out python3 tools/dev.py --cores 2 test --filter Balance.Baseline. > .local/logs/sweep-$name-$s.log 2>&1
      echo "$name $s test exit $?"
    done
  done
}
export -f inner
flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock bash -c 'inner "$@"' _ "$@"
for spec in "$@"; do IFS=: read -r name rev strategies <<<"$spec"; for s in ${strategies//,/ }; do python3 tools/balance_report.py .local/balance/$name-$s | tail -1; done; done
