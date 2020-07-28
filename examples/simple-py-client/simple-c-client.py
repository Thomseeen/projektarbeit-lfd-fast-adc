import struct
import paho.mqtt.client as mqtt


class MeasurementsRecoder:

  def __init__(self, address, port, client_id, topic, qos, keep_alive):

    self.client = mqtt.Client(client_id=client_id)
    self.client.on_connect = self.on_connect
    self.client.on_message = self.on_message

    self.topic = topic
    self.qos = qos

    self.client.connect(address, port, keep_alive)

  def start(self):
    self.client.loop_forever()

  def on_connect(self, client, userdata, flags, rc):
    print("Connected with result code " + str(rc))
    self.client.subscribe(self.topic, qos=self.qos)

  def on_message(self, client, userdata, msg):
    try:
      # See adc_reading.h in simple-c-client why struc is packed like this
      data = struct.unpack("@HQBI", msg.payload)
      value = data[0] / 0xFFF0 * 1.8
      seq_no = data[1]
      pin_no = data[2]
      status = data[3]
      print(f"Pin {pin_no} reads {value} at {seq_no}")

    except Exception:
      import traceback
      print(traceback.format_exc())


if __name__ == "__main__":
  ADDRESS = "192.168.178.16"
  PORT = 1883
  CLIENTID = "mfd-py"
  TOPIC = "lfd1/#"
  QOS = 0
  KEEP_ALIVE = 30  # s

  measurements_recorder = MeasurementsRecoder(
      ADDRESS,
      PORT,
      CLIENTID,
      TOPIC,
      QOS,
      KEEP_ALIVE,
  )

  measurements_recorder.start()