# Evidence for PR #205: rigged foes (scout, archer, skirmisher, brute, warden), head `0e00730`

Native evidence from `feature/150-foes` at `c9c581be` (the head after it adds only documentation): `index/*.json` record the commit and `dirty: false`.

- `sheets/`: Blender turnarounds (`f02`, `f03`), the ten clips per foe (`f04` to `f08`), foes on stone beside the heroes (`f09`, `f10`), the archer's and brute's override clips beside the shared ones (`f11`, `f12`), bind pose (`f01`).
- `combat/`: all five foes in play on Vigil (`foes-vigil-a`, `-b`), Root (`foes-root-a` to `-c`) and the Vault (`foes-vault-a`, `foes-vault-finale`) at 1920x1080: default zoom yaw 0 and 90, far zoom yaw 0. The skirmisher, brute and warden are drawn through the capture-only `-ExpeditionFoeLooks` override (they are not generated until #188).
- `warden-finale-default.png`, `warden-finale-far.png`: the finale-like Vault board with the warden by the Waystone apse.
- `compare/`: the scout and archer, static versus rigged (`default-seed-battle`, `board-root-r002`), and the new foes rigged versus the engine-primitive fallback (`foes-vigil-b`, `foes-vault-finale`).
- `poses/`: foe pose shots (`tools/capture/foes-poses.json`): Melee (the brute's two-handed swing at three times), Ranged (the archer's draw), Brace, Hit and Death.
- `contrast/`: the contrast table per foe and board (`tools/figure_contrast.py`), as Markdown and JSON.
