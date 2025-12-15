
import pickle

import paho.mqtt.client as mqtt
import serial

SERIAL_PORT = "/dev/cu.SLAB_USBtoUART"
BAUD_RATE = 115200

MQTT_BROKER = "127.0.0.1"
MQTT_PORT = 1883
BASE_TOPIC = "TEAM_{}/weather_data"

def main():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

    client = mqtt.Client()
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_start()

    print("Bridge running... pickling RAW packets to MQTT.")

    try:
        while True:
            line = ser.readline().decode(errors="ignore").strip()
            if not line:
                continue

            if line.startswith("RAW:"):
                raw_data = line[4:].strip()
                print(f"Received raw hex string: {raw_data}")

                raw_bytes = bytes.fromhex(raw_data.replace(" ", ""))
                print(f"Converted to bytes: {raw_bytes}")

                if not raw_bytes:
                    print("Warning: Empty packet, skipping")
                    continue

                # First byte = team number
                team_number = raw_bytes[0]
                print(f"Extracted team number: {team_number}")

                # Make sure it's within range (1–40 for your case)
                if 1 <= team_number <= 40:
                    topic = BASE_TOPIC.format(team_number)
                else:
                    topic = "UNKNOWN_TEAM/weather_data"
                    print(f"⚠️ Unknown team number {team_number}, using fallback topic")

                payload = pickle.dumps(raw_bytes)

                print(f"Publishing {len(payload)} bytes to topic '{topic}'")
                client.publish(topic, payload)

    except KeyboardInterrupt:
        print("\nStopping bridge...")
    finally:
        client.loop_stop()
        client.disconnect()
        ser.close()

if __name__ == "__main__":
    main()

