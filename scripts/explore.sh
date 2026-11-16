#!/usr/bin/env bash
# usage: explore.sh [--build] "name|STRATEGIES|VAR=v VAR=v" ...   (one lock hold; revision 11 sweeps)
set -u
mkdir -p .local/logs
inner() {
  if [ "$1" = "--build" ]; then shift; python3 tools/dev.py --cores 2 build > .local/logs/build-explore.log 2>&1; b=$?; echo "build $b"; [ $b -ne 0 ] && { tail -20 .local/logs/build-explore.log; return 1; }; fi
  for spec in "$@"; do
    IFS='|' read -r name strategies vars <<<"$spec"
    for s in ${strategies//,/ }; do
      out=.local/balance/x-$name-$s; rm -rf "$out"
      env $vars DND_BALANCE_REVISION=11 DND_BALANCE_REWARD=$s DND_BALANCE_OUT=$out python3 tools/dev.py --cores 2 test --filter Balance.Baseline. > .local/logs/x-$name-$s.log 2>&1
      echo "$name $s exit $?"
    done
  done
}
export -f inner
flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock bash -c 'inner "$@"' _ "$@"
