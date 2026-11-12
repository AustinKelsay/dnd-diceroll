# Ranged release: the shooter's footprint box, before and after (`tools/impact_flash.py`, Rec. 709 luma of 8-bit sRGB, peak / mean / pure white (all channels >= 250))

BEFORE = `effect: bolt, phase: release`: the spell bolt the Ranger's attack threw before the fix (50 cd point light at the shooter). AFTER = `effect: arrow, phase: release`: the arrow as `AddAttack` now throws it. CONTROL = `effect: arrow, phase: landing`: the arrow is at the foe, 3-5 cells away, so the shooter's box shows the scene and the figure with no shot near it. All shots hold the Ranger in its Ranged clip (release frame 0.87 s; landing 1.17 s), 4 frames after the shot is fired.

| fixture | res | view | BEFORE (bolt) peak / mean / white | AFTER (arrow) | CONTROL (no shot near) |
| --- | --- | --- | --- | --- | --- |
| cast2-board-vault | 1280x720 | yaw000-default | 255.0 / 191 / 6.46% | 250.9 / 124 / 0.00% | 250.0 / 126 / 0.00% |
| cast2-board-vault | 1280x720 | yaw090-default | 255.0 / 189 / 7.30% | 253.6 / 121 / 0.09% | 253.0 / 118 / 0.13% |
| cast2-board-vault | 1280x720 | yaw180-default | 252.2 / 182 / 0.04% | 252.2 / 105 / 0.00% | 251.9 / 109 / 0.00% |
| cast2-board-vault | 1280x720 | yaw270-default | 255.0 / 173 / 3.45% | 252.1 / 133 / 0.00% | 251.9 / 139 / 0.00% |
| cast2-board-vault | 1280x720 | yaw000-near | 255.0 / 185 / 9.67% | 252.4 / 118 / 1.23% | 252.4 / 118 / 1.26% |
| cast2-board-vault | 1280x720 | yaw090-near | 255.0 / 182 / 3.56% | 254.6 / 123 / 1.87% | 254.6 / 124 / 2.03% |
| cast2-board-vault | 1920x1080 | yaw000-default | 255.0 / 191 / 6.21% | 252.4 / 124 / 0.00% | 251.9 / 125 / 0.05% |
| cast2-board-vault | 1920x1080 | yaw090-default | 255.0 / 190 / 7.66% | 253.8 / 123 / 0.17% | 252.8 / 117 / 0.27% |
| cast2-board-vault | 1920x1080 | yaw180-default | 254.0 / 181 / 0.11% | 252.2 / 106 / 0.00% | 252.5 / 109 / 0.00% |
| cast2-board-vault | 1920x1080 | yaw270-default | 255.0 / 171 / 3.91% | 251.9 / 133 / 0.00% | 252.1 / 139 / 0.00% |
| cast2-board-vault | 1920x1080 | yaw000-near | 255.0 / 184 / 9.64% | 252.9 / 117 / 1.26% | 252.4 / 117 / 1.41% |
| cast2-board-vault | 1920x1080 | yaw090-near | 255.0 / 182 / 3.74% | 254.6 / 122 / 2.05% | 254.6 / 124 / 2.31% |
| cast2-default-seed-battle | 1280x720 | yaw000-default | 254.9 / 176 / 2.67% | 244.7 / 96 / 0.00% | 244.5 / 95 / 0.00% |
| cast2-default-seed-battle | 1280x720 | yaw090-default | 255.0 / 194 / 5.19% | 250.9 / 128 / 0.00% | 251.0 / 128 / 0.00% |
| cast2-default-seed-battle | 1280x720 | yaw180-default | 244.7 / 173 / 0.00% | 244.2 / 113 / 0.00% | 244.4 / 115 / 0.00% |
| cast2-default-seed-battle | 1280x720 | yaw270-default | 254.9 / 165 / 0.30% | 248.1 / 122 / 0.00% | 248.1 / 124 / 0.00% |
| cast2-default-seed-battle | 1280x720 | yaw000-near | 255.0 / 177 / 5.04% | 245.0 / 92 / 0.00% | 246.0 / 93 / 0.00% |
| cast2-default-seed-battle | 1280x720 | yaw090-near | 255.0 / 182 / 2.32% | 251.1 / 121 / 0.00% | 251.2 / 124 / 0.00% |
| cast2-default-seed-battle | 1920x1080 | yaw000-default | 255.0 / 176 / 2.38% | 244.5 / 95 / 0.00% | 244.7 / 95 / 0.00% |
| cast2-default-seed-battle | 1920x1080 | yaw090-default | 255.0 / 193 / 5.16% | 251.1 / 127 / 0.00% | 251.1 / 126 / 0.00% |
| cast2-default-seed-battle | 1920x1080 | yaw180-default | 246.2 / 174 / 0.00% | 244.7 / 113 / 0.00% | 244.9 / 116 / 0.00% |
| cast2-default-seed-battle | 1920x1080 | yaw270-default | 254.9 / 165 / 0.54% | 249.1 / 122 / 0.00% | 249.1 / 124 / 0.00% |
| cast2-default-seed-battle | 1920x1080 | yaw000-near | 255.0 / 176 / 5.03% | 245.9 / 92 / 0.00% | 246.0 / 92 / 0.00% |
| cast2-default-seed-battle | 1920x1080 | yaw090-near | 255.0 / 181 / 2.35% | 251.1 / 121 / 0.00% | 252.3 / 123 / 0.00% |
