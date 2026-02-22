import glob
import os
import re
import subprocess
import time

EMU_PATH = "./build/Gameboy"
ROM_DIR = "./roms/mooneye-gb/acceptance/ppu/"
TRACE_FILE = "trace.log"

TIMEOUT_SECONDS = 20


def parse_registers(line):
    """
    Extract A, B, C, D, E, H, L from a single trace line.
    Returns tuple of ints or None.
    """
    match = re.search(
        r"A:\s*([0-9A-F]+).*?"
        r"B:\s*([0-9A-F]+).*?"
        r"C:\s*([0-9A-F]+).*?"
        r"D:\s*([0-9A-F]+).*?"
        r"E:\s*([0-9A-F]+).*?"
        r"H:\s*([0-9A-F]+).*?"
        r"L:\s*([0-9A-F]+)",
        line,
    )

    if not match:
        return None

    return tuple(int(x, 16) for x in match.groups())


def detect_result_from_line(line):
    regs = parse_registers(line)
    if not regs:
        return None

    A, B, C, D, E, H, L = regs

    # SUCCESS: Fibonacci signature
    if (B, C, D, E, H, L) == (3, 5, 8, 13, 21, 34):
        return "PASS"

    # FAILURE: All registers 0x42
    if (B, C, D, E, H, L) == (0x42,) * 6:
        return "FAIL"

    return None


def run_rom(rom_path):
    if os.path.exists(TRACE_FILE):
        os.remove(TRACE_FILE)

    proc = subprocess.Popen([EMU_PATH, rom_path])

    start_time = time.time()
    file_pos = 0
    result = None

    while time.time() - start_time < TIMEOUT_SECONDS:
        if not os.path.exists(TRACE_FILE):
            time.sleep(0.02)
            continue

        with open(TRACE_FILE, "r") as f:
            f.seek(file_pos)
            new_lines = f.readlines()
            file_pos = f.tell()

        for line in new_lines:
            detected = detect_result_from_line(line)
            if detected:
                result = detected
                break

        if result:
            break

        time.sleep(0.01)

    # Stop emulator
    try:
        proc.terminate()
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()

    return result or "TIMEOUT"


def main():
    roms = sorted(glob.glob(os.path.join(ROM_DIR, "*.gb")))
    results = {}

    for rom in roms:
        name = os.path.basename(rom)
        print(f"Running {name}...")
        result = run_rom(rom)
        results[name] = result
        print(f"  → {result}")

    print("\nSummary:")
    for name, result in results.items():
        print(f"{name}: {result}")


if __name__ == "__main__":
    main()
