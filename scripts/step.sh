#!/usr/bin/env bash
# usage: step.sh NAME [extra test filter]  — build, optional test filter, then the four revision 11 sweeps, inside ONE lock hold.
set -u
name=$1; filter=${2:-}
mkdir -p .local/logs
inner() {
  name=$1; filter=$2
  python3 tools/dev.py --cores 2 build > .local/logs/build-$name.log 2>&1; b=$?; echo "build $b"; tail -1 .local/logs/build-$name.log
  [ $b -ne 0 ] && return 1
  if [ -n "$filter" ]; then python3 tools/dev.py --cores 2 test --filter "$filter" > .local/logs/test-$name.log 2>&1; echo "test $filter $?"; tail -2 .local/logs/test-$name.log | cut -c1-400; fi
  for s in gear reserve honed mixed; do
    out=.local/balance/$name-$s; rm -rf "$out"
    DND_BALANCE_REVISION=11 DND_BALANCE_REWARD=$s DND_BALANCE_OUT=$out python3 tools/dev.py --cores 2 test --filter Balance.Baseline. > .local/logs/sweep-$name-$s.log 2>&1
    echo "$name $s sweep exit $?"
  done
}
export -f inner
flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock bash -c 'inner "$@"' _ "$name" "$filter"
for s in gear reserve honed mixed; do python3 tools/balance_report.py .local/balance/$name-$s | tail -1; done
