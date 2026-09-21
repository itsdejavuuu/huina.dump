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
| `interfaces.txt` | All interfaces per module (instance + vtable + slot count, garbage filtered) and `ClientClass` list with IDs |
| `interfaces.json` | Same interfaces, machine readable |
| `interfaces_vtables.txt` | VTable slots `[0..127]` per interface: absolute + `module+RVA` (`???` = garbage/end) |
| `recv_hierarchy.txt` | Full `RecvTable` tree per class: props with total offsets, types, flags, nested tables |
| `Classes/*.txt` | Per-table and per-class dumps |
| `dump_info.txt` | Modules, ClassID lane, walk stats |
| `dump.log` | Run log |

Dumped offsets: `entity_list`, `local_player`, `render`, `client_state`, `global_vars`, `view_setup`, `view_angles`, `force_jump` (RIP-relative), `bone_matrix`, `studio_hdr` (imm32)

Interfaces: full sweep of every loaded DLL via the interface registry
(`CreateInterface` + registry walk), not just a fixed version list.
Entries with >50% garbage vtable slots are skipped.

## Sample dump

`dump-21.09.2026/` — real output from Garry's Mod x64 (21.09.2026):
252 classes, 5026 netvars, 214 interfaces, 6093 vtable slots.

## Build

Requires CMake 3.20+ and MSVC (x64)

```powershell
cmake -S . -B build
cmake --build build --config Release
```