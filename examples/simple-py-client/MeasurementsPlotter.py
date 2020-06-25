import numpy as np
import matplotlib.pyplot as plt

with open("measurements.npy", "rb") as file:
  measurements = np.load(file)

print(measurements.shape[0])
print(measurements[:, 0][:10])

for cnt in range(measurements.shape[0]):
  if cnt not in measurements[:, 0]:
    print(f"Missing index {cnt}")

plt.plot(measurements[:, 0])

plt.show()
