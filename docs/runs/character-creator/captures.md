# Character creator real-viewport captures (2026-11-02)

Local proposal, outside the approved CONTEXT.md scope. These are real-viewport captures taken with
`FScreenshotRequest::RequestScreenshot` and the real HUD in a `-game -windowed` 1600x900 run. None of them is a mock.

Command (Amaterasu, from the repo root):

```
UnrealEditor.exe DnDDiceRoll.uproject -game -windowed -ResX=1600 -ResY=900 -nosplash -NoSound ^
  -ExpeditionSaveRoot=<scratch>\save -CreatorTrial=<scratch> -abslog=<scratch>\game.log
```

`-CreatorTrial=<dir>` (dev-only, in `AExpeditionPlayerController::TickCreatorTrial`) runs one step every 1.5 s and then quits. The steps:
1. Selects party slot 2 (Nix, prebuilt Rogue).
2. Opens the creator through the `CreateHero` button.
3. Types "Vessa Thorn" through the real key router (`RouteCreatorKey`).
4. Lowers every ability to 8, then raises DEX to 15 and CON to 14 (11 points left).
5. Raises WIS to 15 and CHA to 10 (0 left).
6. Picks Stealth and Perception with Expertise, plus Acrobatics and Investigation.
7. Changes two colours, presses Enter on Confirm, then starts a new expedition.

The log line `CREATOR_TRIAL confirmed open=0 custom=1 name=Vessa Thorn` confirms the slot was written.

| File | Shows |
|---|---|
| 01_title_create_hero_button.png | Title / party builder with the "Create hero" button |
| 02_creator_start.png | Creator at start (slot's class, recommended array, 0 of 27 left) |
| 03_creator_points_spent.png | After spending points: 11 of 27 left, arcane wash on the selected CON row |
| 04_creator_finished_glow.png | Finished sheet: 4/4 skills, 2/2 Expertise, emblem and Confirm glow |
| 05_finished_hero_in_party.png | Title panel: slot 2 is now "Vessa Thorn / Rogue, Custom hero" with colour swatches |
| 06_hero_in_run_hud.png | New run HUD: Vessa Thorn in the party rail at 10/10 HP |

Locations: `C:\Users\Zubal\Dev\_ororo_scratch\rules\creator_trial\` on Amaterasu, mirrored to `/workspace/creator_trial/` on the box. The images are not committed.
