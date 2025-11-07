import keyboard
from paho.mqtt import client as mqtt_client
import threading
from queue import Queue

key_queue = Queue(maxsize=20)

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")

def on_message(client, userdata, msg):
    print(msg.topic+" "+str(msg.payload))

def keyboard_listener():
    def on_key_press(event):
        if event.name in ['left', 'right', 'up', 'down']:
            try:
                key_queue.put_nowait(event.name.upper())
                print(f"Queued: {event.name.upper()} (Queue size: {key_queue.qsize()})")
            except:
                print(f"Queue full! Dropped key press: {event.name.upper()}")
    
    keyboard.on_press(on_key_press)
    keyboard.wait()

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

mqttc.connect("192.168.93.191", 1883, 60)

keyboard_thread = threading.Thread(target=keyboard_listener, daemon=True)
keyboard_thread.start()

publisher_thread = threading.Thread(target=process_key_queue, args=(mqttc,), daemon=True)
publisher_thread.start()

print("Ready! Press arrow keys to send MQTT messages. Press Ctrl+C to exit.")
mqttc.loop_forever()