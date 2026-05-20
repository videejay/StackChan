
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

Uses the project `partitions.csv`: factory/OTA app region starts at **0x20000** with **~7 MiB** per `ota_0` / `ota_1` slot; SPIFFS **`assets`** is at **0xE00000** (~1.9 MiB). This table is **not** byte-identical to [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) `custom_16Mb.csv` (larger app partitions, smaller SPIFFS). Use the offsets below when flashing `generated_assets.bin` manually.

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
   For **this** repo’s `partitions.csv`, the SPIFFS **`assets`** partition starts at **0xE00000**. Put the device in **download mode**, then:

   ```bash
   python -m esptool --chip esp32s3 -p PORT write_flash 0xe00000 build/generated_assets.bin
   ```

   Replace `PORT` with your serial port (e.g. `COM7` on Windows). If you still use Launcher’s stock table with SPIFFS at **0xD00000**, use that offset instead — do **not** mix table and address.

**Sizes:** `generated_assets.bin` must not exceed the SPIFFS partition size in **`partitions.csv`** (**0x1D0000** bytes ≈ **1.9 MiB** for this project). If the build fails or the file is too large, shrink emoji/fonts in `main/CMakeLists.txt` for `CONFIG_BOARD_TYPE_M5STACK_STACK_CHAN` and rebuild.

**Partition label:** xiaozhi code looks for partition **`assets`** first, then **`spiffs`** (patch under `patches/xiaozhi-esp32.patch`), so it finds Launcher’s **`spiffs`** slot after restore.

---

### StackChan UI assets (LittleFS / SD)

Separate from **`generated_assets.bin`**: UI icons and backgrounds can use **embedded fallbacks** (build time) and optional overrides from **LittleFS** (defaults: mount `/launcherfs`, partition label `userdata`) or SD—see `main/assets/assets_bin/README.md`.

Run `idf.py reconfigure` after pulling — the `joltwallet/littlefs` component is required for optional LittleFS mounting.

## Troubleshooting

### `esp_ota_ops: not found otadata` / wrong partition labels (e.g. `app1` vs `ota_0`)

The flash layout must match [partitions.csv](partitions.csv). A device flashed with an older or different table can run an app but fail OTA state queries. Fix: **full re-flash** so bootloader + partition table + app align:

```bash
idf.py erase-flash flash
```

Then restore SPIFFS / `generated_assets.bin` if you use Launcher (see above).

### `esp_littlefs: partition "userdata" could not be found`

[partitions.csv](partitions.csv) defines a **`userdata`** LittleFS partition for Launcher UI assets. If the flash was never written with this table (old image or partial flash), mount fails. Fix: **erase flash and full re-flash** with this project’s partition table:

```bash
idf.py erase-flash flash
```

Erasing clears **NVS** (Wi‑Fi, xiaozhi settings). Restore SPIFFS / `generated_assets.bin` if you use Launcher.

### Serial: `WARM_REBOOT` / warm reboot loop

`requestWarmReboot` logs `WARM_REBOOT` plus the target app index. If you see this without intending to leave xiaozhi, check accidental **home** button taps (debounced 2s) or other callers of `requestWarmReboot`.
