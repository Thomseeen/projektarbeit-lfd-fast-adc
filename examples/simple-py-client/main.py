import struct
import paho.mqtt.client as mqtt

ADDRESS = "192.168.178.16"
PORT = 1883
CLIENTID = "mfd-py"
TOPIC = "lfd/#"
QOS = 0
KEEP_ALIVE = 30  # s


def on_connect(client, userdata, flags, rc):
  print("Connected with result code " + str(rc))
  client.subscribe(TOPIC, qos=QOS)


def on_message(client, userdata, msg):
  print(f"Data recieved with length {len(msg.payload)}")
  try:
    # See adc_reading.h in simple-c-client why struc is packed like this
    data = struct.unpack("@HQBQX", msg.payload)
    value = data[0] / 0xFFF0 * 1.8
    seq_no = data[1]
    pin_no = data[2]
    status = data[3]
    print(f"Pin {pin_no} reads {value} at {seq_no}")
  except Exception:
    import traceback
    print(traceback.format_exc())


client = mqtt.Client(client_id=CLIENTID)
client.on_connect = on_connect
client.on_message = on_message

client.connect(ADDRESS, PORT, KEEP_ALIVE)

client.loop_forever()