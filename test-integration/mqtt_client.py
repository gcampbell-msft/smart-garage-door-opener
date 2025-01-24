import paho.mqtt.client as mqtt
import time
import wifi_credentials

# Define the callback functions
def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    client.subscribe("garage_door/buttonpress")

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    print(f"Message received: {msg.topic} {msg.payload.decode()}")
    if (payload == "CLOSE"):
        client.publish("garage_door/status", "closing")
        time.sleep(5)
        client.publish("garage_door/status", "closed")
    elif (payload == "OPEN"):
        client.publish("garage_door/status", "opening")
        time.sleep(5)
        client.publish("garage_door/status", "open")

# Note if I give message "None", it will put Home Assistant Cover in unknown state. I should 
# send "None" if a button is pressed and we don't have confidence after a certain amount of time. 
# For example, if I press close and the sensor doesn't show that it's closed after a certain amount
# of time, send "None". 

# Create an MQTT client instance
client = mqtt.Client()

# Assign the callback functions
client.on_connect = on_connect
client.on_message = on_message

# Set username and password
client.username_pw_set(username=wifi_credentials.username, password=wifi_credentials.password)

# Connect to the broker
client.connect(wifi_credentials.ip, 1883, keepalive=60)

# Publish a message
client.publish("garage_door/buttonpress")

# Start the loop to process network traffic and dispatch callbacks
client.loop_forever()
