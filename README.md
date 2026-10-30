## JSON configuration

Model Swapper reads JSON files whose names end in `_MS.json` from the game's `Data` folder. The format remains an array of swap entries. Existing files using only `OriginalModel`, `Variants`, and the optional `ActiveTimeSpan` continue to work unchanged.

Each entry can now include `BaseFormID`, `ReferenceID`, or both, directly beside the existing fields:

```json
[
  {
    "OriginalModel": "Example\\Original.nif",
    "Variants": ["Example\\Replacement.nif"],
    "BaseFormID": "Example.esp|001234",
    "ReferenceID": "Example.esp|005678"
  }
]
```

- `BaseFormID` limits the entry to references with that base form. For NPC skin models, this is the owning NPC's base form ID, rather than the armor-addon's ID.
- `ReferenceID` limits the entry to that one placed or generated reference.
- If both fields are present, both must match. Omitted fields impose no restriction.

The model path must still match `OriginalModel`. Matching entries for the same model contribute their variants to the eligible pool, including entries without ID restrictions. Restricted entries do not have priority over unrestricted entries. Selection remains random, seeded by the reference ID, and `ActiveTimeSpan` keeps its existing behavior.

Both ID fields accept these forms:

| Value | Meaning |
| --- | --- |
| `4660` | A positive JSON integer: full runtime form ID in decimal. |
| `"0x01001234"` or `"01001234"` | Full runtime form ID in hexadecimal, with an optional `0x` or `0X` prefix. |
| `"Example.esp|001234"` | Plugin filename followed by its local form ID in hexadecimal. The current load order is resolved automatically. |
| `"Example.esl|ABC"` | The same plugin/local-ID format for a light plugin. |

IDs must be nonzero and fit within 32 bits. Plugin-local IDs must fit within 24 bits (`FFFFFF`) for regular plugins or 12 bits (`FFF`) for light plugins, including ESP files flagged as light. A numeric JSON value is decimal; a string ID is hexadecimal.

Invalid or unresolved IDs are logged and that entry is skipped. Other valid entries in the file continue loading.

Prefer the plugin/local-ID format for installed plugins: raw full runtime IDs can change when the load order changes. Generated reference IDs are save-dependent.

[TargetedSwaps_MS.json](examples/TargetedSwaps_MS.json) contains unchanged legacy, base-only, reference-only, and combined examples. It is a template and is not loaded automatically from `examples`. Replace the placeholder plugin names, IDs, and model paths, then copy it into the game's `Data` folder. If you rename it, keep the `_MS.json` suffix.

## Environment variables

[How to set up envioriment variables](https://gist.github.com/Thiago099/b45ec7832fb754325b29a61006bcd10c)

- COMMONLIB_SSE_FOLDER (optional; overrides the bundled `extern/CommonLibSSE-NG` submodule)
- CCOMPILER_PATH (example E:/Program Files/VisualStudio/IDE/VC/Tools/MSVC/14.40.33807/bin/Hostx64/x64/cl.exe)

  To use a shared CommonLib clone, set `COMMONLIB_SSE_FOLDER` to its path. It must support Skyrim 1.7.104; older clones are detected during configuration and the bundled copy is used instead. Initialize the bundled copy with `git submodule update --init --recursive`.

## Skyrim compatibility

Builds require a CommonLibSSE-NG version that recognizes Skyrim 1.7 as AE and reads its address-library format. The existing SE, AE, and VR build options and model-loading hooks remain enabled. Comparing the supplied 1.7.104.0 and 1.6.1179.0 executables confirmed that both use slots `0x6A` and `0x6D` for `Load3D` and `ShouldBackgroundClone`, so the existing hook positions are preserved.

Skyrim 1.7 requires matching SKSE and Address Library installations. Binary inspection and a successful build do not replace an in-game test.
  
## Optional ouput folder optional variables

- SKYRIM_FOLDER
- WILDLANDER_OWRT_FOLDER
- SKYRIM_OWRT_FOLDER
- SKYRIM_MODS_FOLDER2
- SKYRIM_MODS_FOLDER
