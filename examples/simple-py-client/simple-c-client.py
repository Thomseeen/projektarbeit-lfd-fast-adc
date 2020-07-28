import struct
import numpy as np
import paho.mqtt.client as mqtt


class MeasurementsRecoder:

  def __init__(self, address, port, client_id, topic, qos, keep_alive, measurements_cnt):
    self.measurements_indexer = 0
    self.measurements = np.zeros((measurements_cnt, 2))

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
      self.measurements[self.measurements_indexer][0] = seq_no
      self.measurements[self.measurements_indexer][1] = value
      self.measurements_indexer += 1

      if self.measurements_indexer == MEASUREMENTS_CNT:
        self.client.disconnect()
        with open("measurements.npy", "wb") as file:
          np.save(file, self.measurements)

    except Exception:
      import traceback
      print(traceback.format_exc())


if __name__ == "__main__":
  ADDRESS = "192.168.178.16"
  PORT = 1883
  CLIENTID = "mfd-py"
  TOPIC = "lfd/#"
  QOS = 0
  KEEP_ALIVE = 30  # s
  MEASUREMENTS_CNT = 50000  # @100Hz with 1 pin -> 500s = 8.3min

  measurements_recorder = MeasurementsRecoder(ADDRESS, PORT, CLIENTID, TOPIC, QOS, KEEP_ALIVE,
                                              MEASUREMENTS_CNT)

  measurements_recorder.start()