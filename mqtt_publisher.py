import serial

SERIAL_PORT = "/dev/cu.SLAB_USBtoUART"  # change as needed
BAUD_RATE = 115200

def main():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print("Listening for LoRa packets...")

    try:
        while True:
            line = ser.readline().decode(errors="ignore").strip()
            if not line:
                continue

            if line.startswith("RAW:"):
                raw_data = line[4:].strip()
                print(f"Received raw hex string: {raw_data}")

                # Convert hex string to bytes
                try:
                    raw_bytes = bytes.fromhex(raw_data.replace(" ", ""))
                except ValueError:
                    print("⚠️ Invalid hex string, skipping")
                    continue

                # Print bytes as hex
                hex_str = " ".join(f"{b:02X}" for b in raw_bytes)
                print(f"HEX: {hex_str}")

                # Print as ASCII (replace non-printable with '.')
                ascii_str = "".join((chr(b) if 32 <= b <= 126 else ".") for b in raw_bytes)
                print(f"ASCII: {ascii_str}\n")

    except KeyboardInterrupt:
        print("\nStopping listener...")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
