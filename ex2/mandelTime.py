import numpy as np
import matplotlib.pyplot as plt
import os
import time

# Define the range of n and m
n_values = range(1, 6, 1)  # n from 1 to 6
m_values = range(1, 9, 1)  # m from 1 to 70 with a step of 10

# Initialize a 2D array to store average times
average_times = np.zeros((len(n_values), len(m_values)))

# Compute average time for each combination of n and m
for i, n in enumerate(n_values):
    for j, m in enumerate(m_values):
        times = []
        for _ in range(2):  # Run 2 attempts for each combination
            start_time = time.time()
            os.system(f"/home/emhadzi/Documents/oslab/ex2/mandel {n} {m}")
            end_time = time.time()
            times.append(end_time - start_time)
        average_times[i, j] = np.mean(times)

# Create a 3D plot
fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')

# Create meshgrid for n and m
n_mesh, m_mesh = np.meshgrid(n_values, m_values, indexing='ij')

# Plot the surface
ax.plot_surface(n_mesh, m_mesh, average_times, cmap='viridis')

# Label axes
ax.set_xlabel('n (Number of Threads)')
ax.set_ylabel('m (Buffer Size)')
ax.set_zlabel('Average Time (s)')

# Show the plot
plt.title('Average Time for Mandelbrot Calculation')
plt.show()
