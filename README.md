## JSON configuration

Model Swapper reads JSON files whose names end in `_MS.json` from the game's `Data` folder. The format remains an array of swap entries. Existing files using only `OriginalModel`, `Variants`, and the optional `ActiveTimeSpan` continue to work unchanged.

`OriginalModel` accepts a model-path string or a nonempty array of model-path strings. An array matches any listed path. You can omit `OriginalModel` when `BaseFormID`, `ReferenceID`, or both are present; the IDs then select the models to replace. If you include `OriginalModel`, its paths provide an additional filter.

Each entry can include `BaseFormID`, `ReferenceID`, `ExcludeBaseFormID`, and `ExcludeReferenceID`, directly beside the existing fields. Each accepts a single ID or an array of IDs:

```json
[
  {
    "OriginalModel": ["Example\\Original.nif", "Example\\OtherOriginal.nif"],
    "Variants": ["Example\\Replacement.nif"],
    "BaseFormID": ["Example.esp|001234", "Example.esp|001235"],
    "ReferenceID": ["Example.esp|005678", "Example.esp|005679"],
    "ExcludeBaseFormID": "Example.esp|001235",
    "ExcludeReferenceID": ["Example.esp|005679", "Example.esp|005680"]
  }
]
```

For example, this entry replaces models for a specific base and reference without requiring their original paths:

```json
[
  {
    "Variants": ["Example\\Replacement.nif"],
    "BaseFormID": "Example.esp|001234",
    "ReferenceID": "Example.esp|005678"
  }
]
```

- `BaseFormID` limits the entry to references with any listed base form. For NPC skin models, this is the owning NPC's base form ID, rather than the armor-addon's ID.
- `ReferenceID` limits the entry to any listed placed or generated reference.
- If both inclusion fields are present, each must match one of its IDs.
- `ExcludeBaseFormID` and `ExcludeReferenceID` reject the entry when either matches any listed ID, even if the inclusion fields match. They can also be used without inclusion fields.
- Omitted fields impose no restriction. An empty inclusion array matches nothing; an empty exclusion array excludes nothing.
- Without either inclusion field, `OriginalModel` is required, including for entries that use only exclusion fields. Empty paths, empty model arrays, and model arrays containing non-string values are invalid.

When `OriginalModel` is present, the model path must match one of its paths. Matching entries contribute their variants to the eligible pool, including entries without ID restrictions. Exclusions affect only their own entry; other matching entries can still supply variants. Restricted entries do not have priority over unrestricted entries. Selection remains random, seeded by the reference ID, and `ActiveTimeSpan` keeps its existing behavior.

All four ID fields accept these forms, either individually or as elements of a flat array. Arrays may mix these forms:

| Value | Meaning |
| --- | --- |
| `4660` | A positive JSON integer: full runtime form ID in decimal. |
| `"0x01001234"` or `"01001234"` | Full runtime form ID in hexadecimal, with an optional `0x` or `0X` prefix. |
| `"Example.esp|001234"` | Plugin filename followed by its local form ID in hexadecimal. The current load order is resolved automatically. |
| `"Example.esl|ABC"` | The same plugin/local-ID format for a light plugin. |

IDs must be nonzero and fit within 32 bits. Plugin-local IDs must fit within 24 bits (`FFFFFF`) for regular plugins or 12 bits (`FFF`) for light plugins, including ESP files flagged as light. A numeric JSON value is decimal; a string ID is hexadecimal.

Invalid or unresolved IDs, including any invalid array element, are logged and the entire entry is skipped. Other valid entries in the file continue loading.

Prefer the plugin/local-ID format for installed plugins: raw full runtime IDs can change when the load order changes. Generated reference IDs are save-dependent.

[TargetedSwaps_MS.json](examples/TargetedSwaps_MS.json) contains legacy, ID-only, model-array, ID-array, and exclusion examples. It is a template and is not loaded automatically from `examples`. Replace the placeholder plugin names, IDs, and model paths, then copy it into the game's `Data` folder. If you rename it, keep the `_MS.json` suffix.

## Qt6 configuration editor

`ModelSwapperEditor.exe` provides a form for each swap entry. Copy the executable into Skyrim's `Data` folder and launch it there. It uses the executable's folder regardless of the shortcut's working directory. You can also select a different Data folder in the application or launch it with `--data-dir "D:\Games\Skyrim\Data"`.

The file list discovers all `_MS.json` files recursively. The plugin itself reads files directly inside `Data`; configurations in subfolders are shown for editing but must be moved to `Data` to be loaded by the plugin. The editor's filename search and discovery are case insensitive; use the exact `_MS.json` suffix when creating files for the plugin.

Select a file, then a swap entry. You can create configurations and add, duplicate, or remove entries. The forms cover original models, replacement variants, all four inclusion/exclusion ID filters, and `ActiveTimeSpan` (`dd-MM~dd-MM`). Optional-field checkboxes control whether a property is saved. Original models and IDs support single values or lists, including empty ID lists.

Each plugin/local ID has an editable dropdown populated with `.esm`, `.esp`, and `.esl` files in the Data folder, plus a separate hexadecimal Form ID input. Plugin names absent from the folder remain editable. Full runtime IDs can also be entered as hexadecimal strings or decimal numbers. The editor checks ID syntax and ranges; the game resolves load order, record existence, and ESP light-plugin flags when loading the configuration.

Additional JSON properties have recursive forms for objects, lists, text, numbers, booleans, and null. Saving preserves these properties and the scalar/list forms of existing fields, and formats the file as indented JSON. Invalid configuration values produce errors before saving. Unsaved changes are protected when switching files or closing; saves also detect external file changes and replace the file atomically.

The editor builds alongside the plugin, including when building the `ModelSwapper` target explicitly:

```powershell
cmake --preset release
cmake --build build/release --target ModelSwapper
```

Run these commands from an x64 Visual Studio developer environment with `VCPKG_ROOT` set. The vcpkg manifest includes Qt6 Widgets (`qtbase`). The existing static Windows preset builds Qt into the executable. The resulting editor is at `build/release/editor/ModelSwapperEditor.exe`. It stays in the build folder; the plugin's automatic game/mod-folder copy only deploys the plugin DLL and its debug symbols. Copy the editor to `Data` manually when ready.

To build the editor independently with an existing Qt6 installation:

```powershell
cmake -S editor -B build/editor -DCMAKE_PREFIX_PATH="C:/Qt/6.x/msvc2022_64"
cmake --build build/editor --config Release
```

Use a Qt installation matching your compiler and runtime. For a shared Qt build, distribute its runtime libraries with the editor following [Qt's Windows deployment instructions](https://doc.qt.io/qt-6/windows-deployment.html). The normal static preset does not need separate Qt DLLs.

Editor tests are available with `-DMS_EDITOR_BUILD_TESTS=ON`; run `ctest --test-dir build/release/editor --output-on-failure` after building the test targets, or use your standalone editor build directory. The tests cover file discovery, JSON validation and preservation, form controls, and unsaved-change handling.

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
