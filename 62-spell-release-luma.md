# Caster release and flight: the caster's footprint box, before and after (`tools/impact_flash.py`, Rec. 709 luma of 8-bit sRGB; peak luma / mean luma / pure-white share, all channels >= 250)

BEFORE = the full 50 cd bolt light at release (ramp off). AFTER = the ramped-in light of this PR (`BoltRampEnd` 0.5, squared smoothstep), the casters' own tinted bolts (the sorcerer's, warlock's and druid's ranged basic attack is a spell bolt now). 108 release boxes. The no-light arrow-release controls (a sorcerer, warlock and druid shooting arrows) read 244 to 254.6 peak: the floor of these boxes (grid lines, HP bars, pale trim); the lead accepted it as scene content.

| Caster | boxes | max white share before / after | boxes over 1% white before / after | mean of mean luma before / after | max peak before / after |
|---|---|---|---|---|---|
| Cleric | 20 | 35.2% / 0.40% | 16 / 0 | 190 / 158 | 255.0 / 254.6 |
| Druid | 20 | 8.8% / 0.13% | 11 / 0 | 173 / 133 | 255.0 / 254.6 |
| Sorcerer | 20 | 6.8% / 0.00% | 8 / 0 | 160 / 112 | 255.0 / 254.6 |
| Warlock | 20 | 7.2% / 0.16% | 10 / 0 | 164 / 137 | 255.0 / 254.6 |
| Wizard | 28 | 28.8% / 0.65% | 23 / 0 | 179 / 142 | 255.0 / 253.4 |

### 1920x1080: release, shooter box, peak luma / mean luma / pure-white share
| fixture | shot | before (full light) | after |
|---|---|---|---|
| board-vault | fire-release-wizard-yaw000-default | 255 / 197 / 16.67% | 252 / 161 / 0.00% |
| board-vault | fire-release-wizard-yaw090-default | 255 / 188 / 10.40% | 253 / 169 / 0.17% |
| board-vault | fire-release-wizard-yaw180-default | 253 / 160 / 0.04% | 252 / 130 / 0.00% |
| board-vault | fire-release-wizard-yaw270-default | 255 / 170 / 6.45% | 252 / 150 / 0.05% |
| board-vault | missile-release-wizard-yaw000-default | 255 / 200 / 28.09% | 252 / 159 / 0.00% |
| board-vault | spell-release-cleric-yaw000-default | 255 / 191 / 33.36% | 253 / 159 / 0.00% |
| board-vault | spell-release-cleric-yaw090-default | 255 / 197 / 6.88% | 255 / 176 / 0.40% |
| board-vault | spell-release-cleric-yaw180-default | 254 / 204 / 0.00% | 253 / 155 / 0.00% |
| board-vault | spell-release-cleric-yaw270-default | 255 / 192 / 9.55% | 253 / 169 / 0.00% |
| board-vault | fire-release-wizard-yaw000-near | 255 / 185 / 16.22% | 253 / 147 / 0.65% |
| board-vault | spell-release-cleric-yaw000-near | 255 / 181 / 34.75% | 254 / 158 / 0.00% |
| default-seed-battle | fire-release-wizard-yaw000-default | 255 / 174 / 5.32% | 242 / 124 / 0.00% |
| default-seed-battle | fire-release-wizard-yaw090-default | 255 / 195 / 4.14% | 251 / 173 / 0.00% |
| default-seed-battle | fire-release-wizard-yaw180-default | 248 / 160 / 0.00% | 248 / 136 / 0.00% |
| default-seed-battle | fire-release-wizard-yaw270-default | 255 / 153 / 1.11% | 249 / 129 / 0.00% |
| default-seed-battle | missile-release-wizard-yaw000-default | 255 / 179 / 13.19% | 242 / 113 / 0.00% |
| default-seed-battle | spell-release-cleric-yaw000-default | 255 / 189 / 17.55% | 251 / 145 / 0.00% |
| default-seed-battle | spell-release-cleric-yaw090-default | 255 / 193 / 4.48% | 249 / 170 / 0.00% |
| default-seed-battle | spell-release-cleric-yaw180-default | 251 / 180 / 0.00% | 251 / 159 / 0.00% |
| default-seed-battle | spell-release-cleric-yaw270-default | 255 / 189 / 6.92% | 251 / 156 / 0.00% |
| default-seed-battle | fire-release-wizard-yaw000-near | 255 / 168 / 6.75% | 251 / 113 / 0.03% |
| default-seed-battle | spell-release-cleric-yaw000-near | 255 / 180 / 16.83% | 251 / 132 / 0.00% |
| cast3-board-vault | spell-release-sorcerer-yaw000-default | 255 / 155 / 5.65% | 252 / 107 / 0.00% |
| cast3-board-vault | spell-release-sorcerer-yaw090-default | 255 / 150 / 1.45% | 253 / 123 / 0.00% |
| cast3-board-vault | spell-release-sorcerer-yaw180-default | 252 / 173 / 0.00% | 252 / 108 / 0.00% |
| cast3-board-vault | spell-release-sorcerer-yaw270-default | 255 / 153 / 0.49% | 255 / 102 / 0.00% |
| cast3-board-vault | spell-release-warlock-yaw000-default | 255 / 167 / 7.16% | 255 / 141 / 0.02% |
| cast3-board-vault | spell-release-warlock-yaw090-default | 255 / 175 / 0.66% | 255 / 149 / 0.00% |
| cast3-board-vault | spell-release-warlock-yaw180-default | 255 / 147 / 0.15% | 255 / 138 / 0.06% |
| cast3-board-vault | spell-release-warlock-yaw270-default | 255 / 167 / 1.72% | 255 / 138 / 0.02% |
| cast3-board-vault | spell-release-druid-yaw000-default | 255 / 179 / 6.07% | 253 / 135 / 0.00% |
| cast3-board-vault | spell-release-druid-yaw090-default | 255 / 173 / 2.09% | 255 / 150 / 0.13% |
| cast3-board-vault | spell-release-druid-yaw180-default | 253 / 187 / 0.00% | 253 / 125 / 0.00% |
| cast3-board-vault | spell-release-druid-yaw270-default | 255 / 177 / 2.36% | 254 / 148 / 0.02% |
| cast3-board-vault | spell-release-sorcerer-yaw000-near | 255 / 150 / 6.78% | 252 / 100 / 0.00% |
| cast3-board-vault | spell-release-warlock-yaw000-near | 255 / 154 / 6.49% | 255 / 136 / 0.16% |
| cast3-board-vault | spell-release-druid-yaw000-near | 255 / 166 / 8.29% | 254 / 131 / 0.00% |
| cast3-board-vault | fire-release-wizard-yaw000-default | 255 / 194 / 16.20% | 252 / 158 / 0.00% |
| cast3-default-seed-battle | spell-release-sorcerer-yaw000-default | 255 / 168 / 0.87% | 244 / 114 / 0.00% |
| cast3-default-seed-battle | spell-release-sorcerer-yaw090-default | 255 / 158 / 0.08% | 244 / 133 / 0.00% |
| cast3-default-seed-battle | spell-release-sorcerer-yaw180-default | 247 / 175 / 0.00% | 245 / 123 / 0.00% |
| cast3-default-seed-battle | spell-release-sorcerer-yaw270-default | 255 / 162 / 0.21% | 244 / 99 / 0.00% |
| cast3-default-seed-battle | spell-release-warlock-yaw000-default | 255 / 174 / 3.20% | 247 / 125 / 0.00% |
| cast3-default-seed-battle | spell-release-warlock-yaw090-default | 254 / 172 / 0.00% | 248 / 148 / 0.00% |
| cast3-default-seed-battle | spell-release-warlock-yaw180-default | 246 / 141 / 0.00% | 245 / 145 / 0.00% |
| cast3-default-seed-battle | spell-release-warlock-yaw270-default | 255 / 171 / 0.71% | 251 / 131 / 0.00% |
| cast3-default-seed-battle | spell-release-druid-yaw000-default | 255 / 175 / 1.25% | 251 / 118 / 0.00% |
| cast3-default-seed-battle | spell-release-druid-yaw090-default | 255 / 177 / 0.02% | 249 / 153 / 0.00% |
| cast3-default-seed-battle | spell-release-druid-yaw180-default | 251 / 153 / 0.00% | 251 / 123 / 0.00% |
| cast3-default-seed-battle | spell-release-druid-yaw270-default | 255 / 176 / 0.73% | 251 / 135 / 0.00% |
| cast3-default-seed-battle | spell-release-sorcerer-yaw000-near | 255 / 153 / 1.44% | 248 / 103 / 0.00% |
| cast3-default-seed-battle | spell-release-warlock-yaw000-near | 255 / 166 / 4.76% | 249 / 118 / 0.00% |
| cast3-default-seed-battle | spell-release-druid-yaw000-near | 255 / 165 / 2.97% | 251 / 106 / 0.00% |
| cast3-default-seed-battle | fire-release-wizard-yaw000-default | 255 / 174 / 5.58% | 242 / 123 / 0.00% |
### 1280x720: release, shooter box, peak luma / mean luma / pure-white share
| fixture | shot | before (full light) | after |
|---|---|---|---|
| board-vault | fire-release-wizard-yaw000-default | 255 / 197 / 16.93% | 247 / 163 / 0.00% |
| board-vault | fire-release-wizard-yaw090-default | 255 / 189 / 11.33% | 253 / 169 / 0.18% |
| board-vault | fire-release-wizard-yaw180-default | 253 / 163 / 0.04% | 252 / 131 / 0.00% |
| board-vault | fire-release-wizard-yaw270-default | 255 / 172 / 6.38% | 252 / 152 / 0.00% |
| board-vault | missile-release-wizard-yaw000-default | 255 / 200 / 28.85% | 249 / 162 / 0.00% |
| board-vault | spell-release-cleric-yaw000-default | 255 / 191 / 32.44% | 253 / 159 / 0.00% |
| board-vault | spell-release-cleric-yaw090-default | 255 / 198 / 7.14% | 255 / 177 / 0.05% |
| board-vault | spell-release-cleric-yaw180-default | 253 / 204 / 0.00% | 253 / 153 / 0.00% |
| board-vault | spell-release-cleric-yaw270-default | 255 / 194 / 9.37% | 253 / 170 / 0.00% |
| board-vault | fire-release-wizard-yaw000-near | 255 / 186 / 16.32% | 253 / 148 / 0.62% |
| board-vault | spell-release-cleric-yaw000-near | 255 / 181 / 35.15% | 254 / 159 / 0.00% |
| default-seed-battle | fire-release-wizard-yaw000-default | 255 / 174 / 5.40% | 240 / 124 / 0.00% |
| default-seed-battle | fire-release-wizard-yaw090-default | 255 / 196 / 3.90% | 251 / 175 / 0.00% |
| default-seed-battle | fire-release-wizard-yaw180-default | 247 / 162 / 0.00% | 246 / 138 / 0.00% |
| default-seed-battle | fire-release-wizard-yaw270-default | 255 / 154 / 0.94% | 247 / 130 / 0.00% |
| default-seed-battle | missile-release-wizard-yaw000-default | 255 / 179 / 13.73% | 243 / 114 / 0.00% |
| default-seed-battle | spell-release-cleric-yaw000-default | 255 / 190 / 18.35% | 250 / 146 / 0.00% |
| default-seed-battle | spell-release-cleric-yaw090-default | 255 / 193 / 4.05% | 250 / 170 / 0.00% |
| default-seed-battle | spell-release-cleric-yaw180-default | 251 / 180 / 0.00% | 251 / 157 / 0.00% |
| default-seed-battle | spell-release-cleric-yaw270-default | 255 / 190 / 6.69% | 251 / 157 / 0.00% |
| default-seed-battle | fire-release-wizard-yaw000-near | 255 / 169 / 6.86% | 251 / 114 / 0.02% |
| default-seed-battle | spell-release-cleric-yaw000-near | 255 / 180 / 16.94% | 251 / 133 / 0.00% |
| cast3-board-vault | spell-release-sorcerer-yaw000-default | 255 / 155 / 5.08% | 249 / 107 / 0.00% |
| cast3-board-vault | spell-release-sorcerer-yaw090-default | 255 / 151 / 1.50% | 251 / 125 / 0.00% |
| cast3-board-vault | spell-release-sorcerer-yaw180-default | 252 / 175 / 0.00% | 252 / 108 / 0.00% |
| cast3-board-vault | spell-release-sorcerer-yaw270-default | 255 / 154 / 0.45% | 255 / 102 / 0.00% |
| cast3-board-vault | spell-release-warlock-yaw000-default | 255 / 167 / 6.94% | 255 / 141 / 0.00% |
| cast3-board-vault | spell-release-warlock-yaw090-default | 255 / 175 / 0.70% | 254 / 148 / 0.00% |
| cast3-board-vault | spell-release-warlock-yaw180-default | 254 / 149 / 0.05% | 254 / 138 / 0.00% |
| cast3-board-vault | spell-release-warlock-yaw270-default | 255 / 168 / 1.71% | 255 / 140 / 0.00% |
| cast3-board-vault | spell-release-druid-yaw000-default | 255 / 179 / 4.81% | 253 / 136 / 0.00% |
| cast3-board-vault | spell-release-druid-yaw090-default | 255 / 175 / 1.76% | 255 / 152 / 0.05% |
| cast3-board-vault | spell-release-druid-yaw180-default | 253 / 187 / 0.00% | 253 / 123 / 0.00% |
| cast3-board-vault | spell-release-druid-yaw270-default | 255 / 177 / 1.96% | 253 / 147 / 0.00% |
| cast3-board-vault | spell-release-sorcerer-yaw000-near | 255 / 151 / 6.57% | 252 / 101 / 0.00% |
| cast3-board-vault | spell-release-warlock-yaw000-near | 255 / 154 / 6.45% | 255 / 136 / 0.07% |
| cast3-board-vault | spell-release-druid-yaw000-near | 255 / 167 / 8.77% | 254 / 132 / 0.00% |
| cast3-board-vault | fire-release-wizard-yaw000-default | 255 / 194 / 15.44% | 246 / 157 / 0.00% |
| cast3-default-seed-battle | spell-release-sorcerer-yaw000-default | 255 / 167 / 0.76% | 244 / 115 / 0.00% |
| cast3-default-seed-battle | spell-release-sorcerer-yaw090-default | 255 / 157 / 0.06% | 248 / 131 / 0.00% |
| cast3-default-seed-battle | spell-release-sorcerer-yaw180-default | 246 / 176 / 0.00% | 245 / 123 / 0.00% |
| cast3-default-seed-battle | spell-release-sorcerer-yaw270-default | 255 / 163 / 0.16% | 245 / 101 / 0.00% |
| cast3-default-seed-battle | spell-release-warlock-yaw000-default | 255 / 173 / 3.00% | 247 / 125 / 0.00% |
| cast3-default-seed-battle | spell-release-warlock-yaw090-default | 253 / 172 / 0.00% | 245 / 149 / 0.00% |
| cast3-default-seed-battle | spell-release-warlock-yaw180-default | 245 / 140 / 0.00% | 245 / 144 / 0.00% |
| cast3-default-seed-battle | spell-release-warlock-yaw270-default | 255 / 172 / 0.68% | 251 / 132 / 0.00% |
| cast3-default-seed-battle | spell-release-druid-yaw000-default | 255 / 175 / 0.99% | 250 / 120 / 0.00% |
| cast3-default-seed-battle | spell-release-druid-yaw090-default | 255 / 178 / 0.15% | 250 / 154 / 0.00% |
| cast3-default-seed-battle | spell-release-druid-yaw180-default | 251 / 153 / 0.00% | 251 / 121 / 0.00% |
| cast3-default-seed-battle | spell-release-druid-yaw270-default | 255 / 176 / 0.71% | 250 / 136 / 0.00% |
| cast3-default-seed-battle | spell-release-sorcerer-yaw000-near | 255 / 154 / 1.31% | 247 / 104 / 0.00% |
| cast3-default-seed-battle | spell-release-warlock-yaw000-near | 255 / 166 / 4.64% | 249 / 118 / 0.00% |
| cast3-default-seed-battle | spell-release-druid-yaw000-near | 255 / 165 / 3.10% | 251 / 106 / 0.00% |
| cast3-default-seed-battle | fire-release-wizard-yaw000-default | 255 / 174 / 5.30% | 245 / 125 / 0.00% |
### In-flight shots of the sorcerer, warlock and druid (after only): shooter box and bolt box
| fixture | res | shot | shooter box | bolt box |
|---|---|---|---|---|
| cast3-board-vault | 1280x720 | spell-flight-sorcerer-yaw000-default | 249 / 98 / 0.00% | 252 / 123 / 0.00% |
| cast3-board-vault | 1280x720 | spell-flight-sorcerer-yaw090-default | 252 / 122 / 0.00% | 252 / 78 / 0.00% |
| cast3-board-vault | 1280x720 | spell-flight-sorcerer-yaw000-near | 252 / 91 / 0.00% | 252 / 110 / 0.00% |
| cast3-board-vault | 1280x720 | spell-flight-warlock-yaw000-default | 254 / 135 / 0.00% | 255 / 169 / 3.65% |
| cast3-board-vault | 1280x720 | spell-flight-warlock-yaw090-default | 253 / 148 / 0.00% | 252 / 97 / 0.00% |
| cast3-board-vault | 1280x720 | spell-flight-warlock-yaw000-near | 255 / 132 / 0.00% | 255 / 166 / 4.62% |
| cast3-board-vault | 1280x720 | spell-flight-druid-yaw000-default | 253 / 123 / 0.00% | 252 / 162 / 0.00% |
| cast3-board-vault | 1280x720 | spell-flight-druid-yaw090-default | 255 / 151 / 0.05% | 252 / 96 / 0.00% |
| cast3-board-vault | 1280x720 | spell-flight-druid-yaw000-near | 253 / 127 / 0.00% | 252 / 149 / 0.00% |
| cast3-board-vault | 1920x1080 | spell-flight-sorcerer-yaw000-default | 251 / 97 / 0.00% | 252 / 122 / 0.00% |
| cast3-board-vault | 1920x1080 | spell-flight-sorcerer-yaw090-default | 253 / 123 / 0.00% | 252 / 78 / 0.00% |
| cast3-board-vault | 1920x1080 | spell-flight-sorcerer-yaw000-near | 252 / 91 / 0.00% | 252 / 110 / 0.00% |
| cast3-board-vault | 1920x1080 | spell-flight-warlock-yaw000-default | 255 / 135 / 0.00% | 255 / 169 / 3.86% |
| cast3-board-vault | 1920x1080 | spell-flight-warlock-yaw090-default | 255 / 149 / 0.00% | 252 / 97 / 0.00% |
| cast3-board-vault | 1920x1080 | spell-flight-warlock-yaw000-near | 255 / 132 / 0.00% | 255 / 166 / 4.70% |
| cast3-board-vault | 1920x1080 | spell-flight-druid-yaw000-default | 253 / 122 / 0.00% | 252 / 162 / 0.00% |
| cast3-board-vault | 1920x1080 | spell-flight-druid-yaw090-default | 255 / 150 / 0.18% | 252 / 97 / 0.00% |
| cast3-board-vault | 1920x1080 | spell-flight-druid-yaw000-near | 253 / 126 / 0.00% | 252 / 149 / 0.00% |
| cast3-default-seed-battle | 1280x720 | spell-flight-sorcerer-yaw000-default | 244 / 116 / 0.00% | 244 / 117 / 0.00% |
| cast3-default-seed-battle | 1280x720 | spell-flight-sorcerer-yaw090-default | 248 / 134 / 0.00% | 245 / 96 / 0.00% |
| cast3-default-seed-battle | 1280x720 | spell-flight-sorcerer-yaw000-near | 246 / 105 / 0.00% | 245 / 110 / 0.00% |
| cast3-default-seed-battle | 1280x720 | spell-flight-warlock-yaw000-default | 249 / 133 / 0.00% | 246 / 156 / 0.00% |
| cast3-default-seed-battle | 1280x720 | spell-flight-warlock-yaw090-default | 245 / 152 / 0.00% | 246 / 132 / 0.00% |
| cast3-default-seed-battle | 1280x720 | spell-flight-warlock-yaw000-near | 250 / 130 / 0.00% | 245 / 146 / 0.00% |
| cast3-default-seed-battle | 1280x720 | spell-flight-druid-yaw000-default | 250 / 128 / 0.00% | 244 / 89 / 0.00% |
| cast3-default-seed-battle | 1280x720 | spell-flight-druid-yaw090-default | 251 / 157 / 0.00% | 245 / 131 / 0.00% |
| cast3-default-seed-battle | 1280x720 | spell-flight-druid-yaw000-near | 251 / 117 / 0.00% | 245 / 83 / 0.00% |
| cast3-default-seed-battle | 1920x1080 | spell-flight-sorcerer-yaw000-default | 244 / 114 / 0.00% | 245 / 117 / 0.00% |
| cast3-default-seed-battle | 1920x1080 | spell-flight-sorcerer-yaw090-default | 245 / 134 / 0.00% | 245 / 97 / 0.00% |
| cast3-default-seed-battle | 1920x1080 | spell-flight-sorcerer-yaw000-near | 247 / 104 / 0.00% | 245 / 110 / 0.00% |
| cast3-default-seed-battle | 1920x1080 | spell-flight-warlock-yaw000-default | 250 / 133 / 0.00% | 246 / 156 / 0.00% |
| cast3-default-seed-battle | 1920x1080 | spell-flight-warlock-yaw090-default | 249 / 152 / 0.00% | 246 / 132 / 0.00% |
| cast3-default-seed-battle | 1920x1080 | spell-flight-warlock-yaw000-near | 250 / 130 / 0.00% | 245 / 146 / 0.00% |
| cast3-default-seed-battle | 1920x1080 | spell-flight-druid-yaw000-default | 251 / 127 / 0.00% | 245 / 89 / 0.00% |
| cast3-default-seed-battle | 1920x1080 | spell-flight-druid-yaw090-default | 251 / 156 / 0.00% | 246 / 131 / 0.00% |
| cast3-default-seed-battle | 1920x1080 | spell-flight-druid-yaw000-near | 251 / 117 / 0.00% | 245 / 83 / 0.00% |
