
## Build

### Fetch Dependencies

```bash
python3 ./fetch_repos.py
```

### Tool Chains

[ESP-IDF v5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/index.html)

### Build

```bash
idf.py build
```

### Flash (standalone)

Uses the project `partitions.csv`: app in `ota_0` at **0x200000** (8 MB), custom assets in partition **`assets`** (SPIFFS, **0xD00000**, ~2.81 MB). This layout matches the physical slots used by [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) on M5Stack CoreS3 (`support_files/custom_16Mb.csv`), so the same `build/stack-chan.bin` and `build/generated_assets.bin` can be installed through Launcher.

```bash
idf.py flash
```

### bmorcelli/Launcher (CoreS3, 16 MB)

1. Flash **Launcher** for your device once (M5Burner, web flasher, or `Launcher-*-cores3*.bin` from Launcher releases).
2. Copy **`build/stack-chan.bin`** and **`build/generated_assets.bin`** to SD / WebUI, or use Launcher OTA if you host the files.
3. Install the firmware from Launcher. When asked about **SPIFFS**, choose **Yes** and select **`generated_assets.bin`** so it is written to Launcher's `spiffs` partition (same flash offset **0xD00000** as this project's `assets` slot).
4. On power-on, Launcher starts first; from its menu, boot the last installed app to run StackChan.

**Assets partition name:** Launcher names the data partition `spiffs`; standalone uses `assets`. The firmware looks up **`assets`** first, then falls back to **`spiffs`** (patch in `patches/xiaozhi-esp32.patch`).

**Size limits after install via Launcher:**

- Application slot: **8 MB** (`ota_0` / Launcher's `app1`).
- Assets: **0x2D0000** bytes (**2,949,120** bytes). If `idf.py build` fails the assets step or `generated_assets.bin` is larger than that, reduce fonts/emoji in `main/CMakeLists.txt` (`CONFIG_BOARD_TYPE_M5STACK_STACK_CHAN` block) and rebuild.

**Verify locally after build:**

```powershell
(Get-Item build\stack-chan.bin).Length -le 8388608
(Get-Item build\generated_assets.bin).Length -le 2949120
```
