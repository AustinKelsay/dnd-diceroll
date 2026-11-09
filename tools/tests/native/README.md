# Portable native tests

Checks of pure C++ headers that need no Unreal, so they run in seconds without an engine slot. They do not establish rendered UI, input or gameplay acceptance.

## `target_chip_layout_test.cpp`

Exercises `Source/DnDDiceRoll/Public/Presentation/TargetChipLayout.h`, the production chip placement, leader clipping and viewport scale used by the HUD. From the repository root:

```sh
g++ -std=c++17 -Wall -Wextra -ISource/DnDDiceRoll/Public tools/tests/native/target_chip_layout_test.cpp -o /tmp/chip && /tmp/chip
g++ -std=c++17 -Wall -Wextra -DNDEBUG -O2 -ISource/DnDDiceRoll/Public tools/tests/native/target_chip_layout_test.cpp -o /tmp/chip && /tmp/chip
```

Both must print `PASS` and exit 0 with no warnings. The checks use a `CHECK` macro, not `assert`, so they still run under `-DNDEBUG`. Run it whenever `TargetChipLayout.h` changes; the HUD's content bounds and scale come from that header.
