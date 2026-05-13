
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

### Launcher ([bmorcelli/Launcher](https://github.com/bmorcelli/Launcher)): app `.bin` plus **xiaozhi assets** (`generated_assets.bin`)

Installing **`build/stack-chan.bin`** from SD / WebUI / OTA usually flashes **only the application**. That is normal: a plain ESP-IDF app binary does **not** contain a SPIFFS image, so Launcher often **never shows** a “install SPIFFS?” step.

Do this **in addition** after the app is installed:

1. **Turn prompts on (optional)**  
   In Launcher go to **CFG** (settings). Find **Avoid & Ask Spiffs** (wording may vary slightly by version). Set it so Launcher **asks** whether to update SPIFFS when it can (see [Functionalities explained](https://github.com/bmorcelli/Launcher/wiki/Functionalities-explained)).  
   Even with **Ask**, you may still get **no** prompt when the chosen file is **only** an app `.bin`—then use step 2 or 3.

2. **Restore SPIFFS from SD (recommended)**  
   - Copy **`build/generated_assets.bin`** from your PC to the SD card (any folder you can open in Launcher’s **SD** browser).  
   - In Launcher **CFG**, use **Restore SPIFFS** (or the recovery option that restores SPIFFS from a file on SD—same idea as **Bkp SPIFFS** but in reverse).  
   - Select **`generated_assets.bin`**. That writes the image into Launcher’s **`spiffs`** data partition.  
   - **Backup tip:** Launcher can save the current SPIFFS to **`/bkp/spiffs.bin`** via **Bkp SPIFFS**; when restoring, it typically lets you pick a backup file—use your `generated_assets.bin` the same way.

3. **USB `esptool` (if SPIFFS restore is awkward)**  
   Only if your CoreS3 still uses Launcher’s **default 16 MB** table where the SPIFFS partition is at **0xD00000** (same as `support_files/custom_16Mb.csv` in the Launcher repo). Put the device in **download mode**, then:

   ```bash
   python -m esptool --chip esp32s3 -p PORT write_flash 0xd00000 build/generated_assets.bin
   ```

   Replace `PORT` with your serial port (e.g. `COM7` on Windows). Do **not** use this offset if your partition table differs.

**Sizes:** `generated_assets.bin` must not exceed the SPIFFS partition size (Launcher **0x2D0000** bytes ≈ **2.81 MiB**). If the build fails or the file is too large, shrink emoji/fonts in `main/CMakeLists.txt` for `CONFIG_BOARD_TYPE_M5STACK_STACK_CHAN` and rebuild.

**Partition label:** xiaozhi code looks for partition **`assets`** first, then **`spiffs`** (patch under `patches/xiaozhi-esp32.patch`), so it finds Launcher’s **`spiffs`** slot after restore.

---

### StackChan UI assets (LittleFS / SD)

Separate from **`generated_assets.bin`**: UI icons and backgrounds can use **embedded fallbacks** (build time) and optional overrides from **LittleFS** (defaults: mount `/launcherfs`, partition label `userdata`) or SD—see `main/assets/assets_bin/README.md`.

Run `idf.py reconfigure` after pulling — the `joltwallet/littlefs` component is required for optional LittleFS mounting.
