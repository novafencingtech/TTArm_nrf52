"""Compile and run portable BLE tests; requires a host C++17 compiler."""
from pathlib import Path
import argparse
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=shutil.which("g++"))
    args = parser.parse_args()
    if not args.compiler:
        parser.error("g++ not found; pass --compiler")
    output = ROOT / ".build/tests/ble_phase1_test.exe"
    output.parent.mkdir(parents=True, exist_ok=True)
    source = ["tests/ble_phase1_test.cpp", "BlePairingState.cpp", "BleIdentity.cpp", "BleGatt.cpp", "BleUi.cpp", "MenuSystem.cpp"]
    subprocess.run([args.compiler, "-std=c++17", "-Itests/fakes", "-I.",
                    *source, "-o", str(output)], cwd=ROOT, check=True)
    subprocess.run([str(output)], cwd=ROOT, check=True)
    output = output.with_name("ble_status_test.exe")
    subprocess.run([args.compiler, "-std=c++17", "-Itests/fakes", "-I.",
                    "tests/ble_status_test.cpp", "BleStatusPublisher.cpp", "BleGatt.cpp", "BleIdentity.cpp",
                    "-o", str(output)], cwd=ROOT, check=True)
    return subprocess.call([str(output)], cwd=ROOT)


if __name__ == "__main__":
    raise SystemExit(main())
