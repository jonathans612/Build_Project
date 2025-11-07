import termios
import tty
import sys

from paho.mqtt import client as mqtt_client
import threading
from queue import Queue

key_queue = Queue(maxsize=20)

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")

def on_message(client, userdata, msg):
    print(msg.topic+" "+str(msg.payload))


def get_input() -> str:
    filedescriptors = termios.tcgetattr(sys.stdin)
    tty.setcbreak(sys.stdin)
    key = sys.stdin.read(1)
    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, filedescriptors)
    return key

def keyboard_listener():
    """Listen for arrow key presses and add them to the queue"""
    print("Keyboard listener started. Press arrow keys (use Ctrl+C to exit)")
    
    while True:
        key = get_input()
        
        # Arrow keys send escape sequences
        if key == '\x1b':  # ESC character
            # Read the next two characters for arrow keys
            key += sys.stdin.read(2)
            
            arrow_map = {
                '\x1b[A': 'UP',
                '\x1b[B': 'DOWN',
                '\x1b[C': 'RIGHT',
                '\x1b[D': 'LEFT'
            }
            
            if key in arrow_map:
                arrow_key = arrow_map[key]
                try:
                    key_queue.put_nowait(arrow_key)
                    print(f"Queued: {arrow_key} (Queue size: {key_queue.qsize()})")
                except:
                    print(f"Queue full! Dropped key press: {arrow_key}")
        elif key == '\x03':  # Ctrl+C
            print("\nExiting...")
            break
    

def process_key_queue(client):
    while True:
        key = key_queue.get()
        
        topic = "keyboard/arrow"
        client.publish(topic, key)
        print(f"Published: {key} to {topic}")
        
        key_queue.task_done()

mqttc = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message

mqttc.connect("192.168.80.1", 1883, 60)

keyboard_thread = threading.Thread(target=keyboard_listener, daemon=True)
keyboard_thread.start()

publisher_thread = threading.Thread(target=process_key_queue, args=(mqttc,), daemon=True)
publisher_thread.start()

print("Ready! Press arrow keys to send MQTT messages. Press Ctrl+C to exit.")
mqttc.loop_forever()