# Original demo character

Copyright (c) 2026 samiosu.

The character atlas, SpriteStudio XML, and mapping are independently authored
for SpriteFace and use the repository's MIT license. They do not include artwork
from the SpriteStudio SDK examples.

`spriteface.sspj` references `character.ssae` and `atlas.ssce`; the latter references
`atlas.png`. Keep these files together. The head is a parent of its eyes and mouth,
so the demo exercises the SDK's transform hierarchy and face-state hook.

`mapping.json` maps normalized eye openness and mouth openness to local scales,
and head angles to rotation/position. This is a simple geometric face, not a
fully rigged illustration. Adapt bindings to your own part or bone names.

To regenerate all files with Python and Pillow:

```sh
python assets/demo/generate_demo.py
```
