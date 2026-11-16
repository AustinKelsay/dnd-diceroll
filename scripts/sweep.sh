#!/usr/bin/env bash
# usage: sweep.sh NAME REVISION [strategies...]  (run from the checkout)
set -u
name=$1; rev=$2; shift 2
strategies=${*:-gear reserve honed mixed}
mkdir -p .local/logs
for s in $strategies; do
  out=.local/balance/$name-$s
  rm -rf "$out"
  DND_BALANCE_REVISION=$rev DND_BALANCE_REWARD=$s DND_BALANCE_OUT=$out \
    flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock python3 tools/dev.py --cores 2 test --filter Balance.Baseline. > .local/logs/sweep-$name-$s.log 2>&1
  echo "$name $s test exit $?"
  python3 tools/balance_report.py "$out" | tail -1
done
