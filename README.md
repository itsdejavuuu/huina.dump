# huina.dump

x64 DLL that dumps Garry's Mod client offsets, netvars and interfaces. Inject, it writes everything to `C:\huina.dump` and unloads itself

## Usage

1. Launch the x64 build of the game.
2. Join any map (dumping from the main menu yields zero class IDs re-inject in game instead)
3. Inject `huina_dump.dll` with any x64 injector
4. Wait for the `Done` message box. Output lands in `C:\huina.dump`

## Output

| File | Contents |
|---|---|
| `offsets.hpp` | `Offsets` (module RVAs) + `NetVars::<Table>` (incl. `prop[N]` arrays), ready to include |
| `offsets.json` | Same offsets, machine readable, with module bases |
| `netvars.txt` / `netvars.json` | Full `[table] prop: offset (type)` flat dump |
| `interfaces.txt` | Grabbed interfaces (instance + vtable) and `ClientClass` list with IDs |
| `Classes/*.txt` | Per-table and per-class dumps |
| `dump_info.txt` | Modules, ClassID lane, walk stats |
| `dump.log` | Run log |

Dumped offsets: `entity_list`, `local_player`, `render`, `client_state`, `global_vars`, `view_setup`, `view_angles`, `force_jump` (RIP-relative), `bone_matrix`, `studio_hdr` (imm32)

## Build

Requires CMake 3.20+ and MSVC (x64)

```powershell
cmake -S . -B build
cmake --build build --config Release
```