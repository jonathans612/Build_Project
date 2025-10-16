import serial
import pynmea2
import time
import sys
from datetime import datetime

# --- 1. Sanity check: establish serial connection ---
try:
    port = serial.Serial("/dev/ttyACM0", baudrate=9600, timeout=1)
    time.sleep(2)  # allow GPS module to initialize
    if not port.is_open:
        sys.exit("Serial port failed to open.")
    print("✅ Serial connection established with GPS.")
except serial.SerialException as e:
    sys.exit(f"❌ Serial error: {e}")

# --- 2. Live GPS read loop with validity and time display ---
no_fix_count = 0
while True:
    try:
        line = port.readline().decode('ascii', errors='replace').strip()
        if not line:
            continue

        if line.startswith('$GPRMC'):
            msg = pynmea2.parse(line)

            # Safely handle timestamp
            if msg.datestamp and msg.timestamp:
                timestamp = f"{msg.datestamp} {msg.timestamp}"
            elif msg.timestamp:
                timestamp = str(msg.timestamp)
            else:
                # fallback to system time if GPS time not available
                timestamp = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S")

            lat, lon = msg.latitude, msg.longitude
            status = msg.status  # 'A' (active) or 'V' (void)

            if status == 'A':
                if abs(lat) > 0.0001 and abs(lon) > 0.0001:
                    print(f"[{timestamp}] Lat {lat:.6f}, Lon {lon:.6f}")
                    no_fix_count = 0
                else:
                    print(f"[{timestamp}] ⚠️ Invalid coordinates (bad data).")
            else:
                no_fix_count += 1
                print(f"[{timestamp}] status={status}.")

        if no_fix_count > 20:
            print("❌ Lost GPS signal or antenna issue.")
            no_fix_count = 0

    except pynmea2.ParseError:
        print("⚠️ NMEA parse error, skipping line.")
    except serial.SerialException:
        print("❌ Serial connection lost.")
        break
    except Exception as e:
        print(f"Error: {e}")
