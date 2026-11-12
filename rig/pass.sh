# pass.sh PREFIX X Y1 Y2 Y3 Y4 LEAVEX LEAVEY  (game already up; pointer mode)
source /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-d35695cc-8d707c6d7c1eb76d/.local/rig/lib.sh
pre=$1; X=$2; H=(0 $3 $4 $5 $6); LX=$7; LY=$8
at(){ ptr m $LX $LY; sleep .4; ptr m $X ${H[$1]}; sleep .5; ptr m $((X+8)) $((${H[$1]}+3)); sleep .6; }
cap(){ sleep 1; shot $pre-$1 0.2; magick $SHOTS/$pre-$1.png -resize 1280x $SHOTS/s-$pre-$1.png; }
sel(){ at $1; ptr c; sleep 1; ptr c; sleep 1; }
sel 4; ptr m $LX $LY; sleep .5; ptr m $((LX+5)) $LY; cap sel4-leave
for h in 1 2 3; do at $h; cap sel4-hov$h; done
sel 1; ptr m $LX $LY; sleep .5; ptr m $((LX+5)) $LY; cap sel1-leave
for h in 2 3 4; do at $h; cap sel1-hov$h; done
ptr m $LX $LY; sleep .5; ptr m $((LX+5)) $LY; sleep 1
for k in 3 2 4 1; do key $k; sleep 1.2; cap keys$k; done
cd $SHOTS; montage $(for n in sel4-leave sel4-hov1 sel4-hov2 sel4-hov3 sel1-leave sel1-hov2 sel1-hov3 sel1-hov4; do echo s-$pre-$n.png; done) -tile 2x -geometry 960x540+2+2 -quality 85 m-$pre-a.jpg
montage $(for n in keys3 keys2 keys4 keys1; do echo s-$pre-$n.png; done) -tile 2x -geometry 960x540+2+2 -quality 85 m-$pre-b.jpg
