import paho.mqtt.client as mqtt
from time import sleep
import time as time

def getBattery():
    with open('/sys/class/power_supply/Battery/capacity') as f:
        return int(f.read())

def getCharge():
    with open('/sys/class/power_supply/Battery/status') as f:
        return "Charging" in f.read()

def on_connect(client, userdata, flags, rc):
    print("Connected with result code: " + str(rc))

def on_message(client, userdata, msg):
    print(msg.topic + " " + str(msg.payload))

# Update these with values suitable for your network.
strBroker = "127.0.0.1"
port = 1883
username = "mqtt-username"
password = "mqtt-password"

topic_publish = 'charge'
topic_subscribe = 'charge_reply'
isCharging = getCharge()

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.username_pw_set(username, password=password) # comment this if you dont use password
client.connect(strBroker, port,600)
client.subscribe(topic_subscribe, qos=0)
client.loop_start()

while True:
    print(time.asctime( time.localtime(time.time()) ))
    if getBattery() < 40 and isCharging == False:
        isCharging = True
        client.publish(topic_publish, payload='1', qos=0) # 40 -> 0
        print("battery: ",getBattery()," charging...")
    elif getBattery() >= 99 and isCharging:
        isCharging = False
        client.publish(topic_publish, payload='0', qos=0) # 99 -> 100
        print("battery is full")
    elif isCharging:
        client.publish(topic_publish, payload='1', qos=0) # 40 -> 99
        print("battery: ",getBattery()," charging...")
    else:
        client.publish(topic_publish, payload='0', qos=0) # 99 -> 40
        print("battery: ",getBattery()," Discharging")

    # sleep 10 secends
    sleep(10)

    # test charge state
    if isCharging != getCharge():
        print("charge state error")
        isCharging = getCharge()
    else:
        print("charge state right")

    # sleep 5 minutes
    sleep(300)
    print("")