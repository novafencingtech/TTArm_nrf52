"""Stage and build using the standard Arduino/Bluefruit installation. No core patches."""
from pathlib import Path
import argparse
import os
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def prepare(directory, ble_disabled=False):
    sketch = directory / "sketch/TTArm_nrf52"
    sketch.mkdir(parents=True, exist_ok=True)
    for path in ROOT.iterdir():
        if path.is_file() and path.suffix in {".ino", ".h", ".cpp", ".c"}:
            shutil.copy2(path, sketch / path.name)
    if ble_disabled:
        config = sketch / "BleConfig.h"
        text = re.sub(r"^#define TTARM_BLE_ENABLED [01]$", "#define TTARM_BLE_ENABLED 0",
                      config.read_text(encoding="utf-8"), flags=re.MULTILINE)
        config.write_text(text, encoding="utf-8")
    return sketch


def main():
    local = Path(os.environ.get("LOCALAPPDATA", str(Path.home() / "AppData/Local")))
    bundled = Path("Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe")
    if not (local / bundled).is_file():
        local = Path.home() / "AppData/Local"
    default_config = Path.home() / ".arduinoIDE/arduino-cli.yaml"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=shutil.which("arduino-cli") or local / bundled)
    parser.add_argument("--config", type=Path, default=default_config if default_config.is_file() else None)
    parser.add_argument("--board", choices=["feather52832", "feather52840"], default="feather52832")
    parser.add_argument("--ble-disabled", action="store_true")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()
    directory = ROOT / ".build" / (args.board + ("-radio-off" if args.ble_disabled else "-standard"))
    sketch = prepare(directory, args.ble_disabled)
    print(f"Prepared {sketch}", flush=True)
    if args.prepare_only:
        return 0
    build_path = (directory / "output").resolve()
    if not build_path.is_relative_to(ROOT.resolve()):
        parser.error("Build output must stay inside the workspace")
    command = [str(args.cli), "compile", "--jobs", str(args.jobs), "--fqbn", f"adafruit:nrf52:{args.board}",
               "--build-path", str(build_path)]
    if args.clean:
        command.append("--clean")
    if args.config:
        command.extend(["--config-file", str(args.config)])
    command.append(str(sketch))
    return subprocess.call(command)


if __name__ == "__main__":
    raise SystemExit(main())
