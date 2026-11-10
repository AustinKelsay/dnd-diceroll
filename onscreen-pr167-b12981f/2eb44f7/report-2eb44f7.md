# PR #167 addendum: package of head `2eb44f7` (merge of staging with #159's camera)

Package `tools/dev.py --cores 2 package` at `2eb44f7f59bfaa87502f58bb090fef1eaec89f71` (0 errors, 0 warnings), binary SHA-256 `ab5d2c8faa036047d2d26ab2e801349b4af2538f98f259f576e50bd30dfacf25`. Lock held for the session (00:30Z to 00:43Z, only my game running). Virtual input, nested headless Hyprland. `kit-wizard-battle` fixture, Hero 1 (Wizard). **Everything in `../report.md` came from the `b12981f` package (binary `f6837ee1...`); only the items below came from this `2eb44f7` package.**

| Item | Result | What was seen |
| --- | --- | --- |
| Plain **A** selects Attack and shows its tooltip | **Pass** | `701-A-1280`, `732-A-1920`: `Attack - Action / At will / ... / Out of range (4)` above the dock, Attack button framed. |
| **Shift+A** pans, neither selects Attack nor shows a tooltip | **Pass** | From a clean turn (`720`, no tooltip) a 0.7 s Shift+A hold panned the camera left and showed no tooltip and no mode change (`721-shiftA-clean-1280`, `733-shiftA-1920`); plain A afterwards shows the tooltip (`722`). With F already selected, Shift+A panned but left F and its Magic Missile tooltip untouched (`711`), i.e. it does not select Attack. |
| F, M, R, P still show their tooltips | **Pass** | 1280x720: `710` Magic Missile (`Out of range (6)`), `712` Sculpted Burst, `713` Rescue (`No downed ally`), `714` Healing Potion (`No wounded target`); 1920x1080: `731`, `734`, `735`, `736`. |
| Dock and tooltip under #159's camera and framing at 1280x720 and 1920x1080; does the tooltip cover anything that matters? | **Pass** | The tooltip sits directly above the dock, centred, about 100 px tall. At the default framing it covers floor only (`700`, `730`: nothing but the lower board edge and a pillar). When the camera is panned or orbited (`721`-`722`, `733`) it can cover board cells at the bottom of the screen, but the camera fit keeps the board clear of the dock in the default pose. It never covers the party cards, the top strip or the Menu. |

Defects: none. Logs (`logs/`): 0 `Error:`, 0 ensures, 0 `LogMaterial`, 0 `Fatal`; the only warning is `SDL_EVENT_QUIT` at close. One extra launch of j1 died at startup with the known SDL SIGFPE rig race; the identical relaunch worked.
