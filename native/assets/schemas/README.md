# Generated Asset Contract

The current `DW4.Content` startup validation expects these catalogs from a complete run of
`extract-native-assets.cmd`:

- `text/dialogue.json`
- `maps/index.json`
- `sprites/field/index.json`
- `monsters/index.json`
- `graphics/index.json`
- `screens/index.json`
- `audio/index.json`

The extractor documents detailed records inside each JSON file. Native loaders must validate those records and convert
them to typed immutable data before gameplay uses them. Generated payloads belong in `../generated/` and are ignored;
do not commit PNG, WAV, VGM, decoded text, map data, or other extracted game content. Runtime and package copies must
remain under ignored build/package paths and are subject to the repository copyright-boundary check.
