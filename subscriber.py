from paho.mqtt import client as mqtt_client

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")
    client.subscribe("keyboard/arrow")
    print("Subscribed to keyboard/arrow topic")

def on_message(client, userdata, msg):
    print(f"Received arrow key: {msg.payload.decode()}")

mqttc = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message

mqttc.connect("192.168.80.1", 1883, 60)

print("Listening for arrow key events...")
mqttc.loop_forever()