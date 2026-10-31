# Ghidra scripts

Java scripts for statically analysing `SkullGirls.exe` with Ghidra's headless
analyzer. They read the game but don't contain any of it. Each script's header
explains its arguments and output in full.

Run a script like this, replacing the project folder and script arguments:

```bash
/opt/ghidra/support/analyzeHeadless <project_dir> <project_name> -process SkullGirls.exe -noanalysis -readOnly -scriptPath scripts -postScript ListReferences.java refs.txt 0 54f2d0
```

Scripts that change the project must run without `-readOnly`.

| Script | Purpose |
|---|---|
| `LabelFromLogStrings` | Names functions after their log messages (changes the project) |
| `CommitFunctionParameters` | Saves decompiler-inferred parameters (changes the project) |
| `ExportVtables` | Lists C++ classes and their virtual functions (can change the project) |
| `ExportStringXrefs` | Finds strings by pattern, and the functions using them |
| `ExportImportCallers` | Finds callers of imported Windows and Steam functions |
| `ListReferences` | Lists what references an address or string |
| `FindInstructions` | Searches instructions by operand value, text or regex |
| `FindMethodCallsOnObject` | Finds methods called on a global object |
| `FindRawStrings` | Finds exact strings in raw memory, with their references |
| `DumpDecomp` | Writes decompiled functions to files |
| `DumpDisassembly` | Writes annotated disassembly to files |
| `DumpValues` | Shows the value at an address as several types |

`keywords.txt` is the search list used with `ExportStringXrefs`.
