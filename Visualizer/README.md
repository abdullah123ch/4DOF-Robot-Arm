# Visualizer Documentation (Processing)

[< Back to Project Root](../README.md)

## Overview

This application creates a **Digital Twin** of the robot arm. It runs on your computer and visualizes the robot's movements in real-time 3D. It is useful for verifying that the math and sensor data are correct before turning on the actual high-power motors.

## Command Reference

| Function                 | Library    | Description                                  |
| :----------------------- | :--------- | :------------------------------------------- |
| `size(w, h, P3D)`        | Processing | Creates window with 3D capabilities.         |
| `pushMatrix()`           | Processing | Saves the current coordinate system.         |
| `popMatrix()`            | Processing | Restores the saved coordinate system.        |
| `translate(x, y, z)`     | Processing | Moves the origin to a new point.             |
| `rotateY(rad)`           | Processing | Rotates the system around the Y-axis.        |
| `box(w, h, d)`           | Processing | Draws a 3D box.                              |
| `lerp(start, stop, amt)` | Processing | Linear Interpolation (for smooth animation). |
| `serialEvent(Serial p)`  | Processing | Interrupt-like event when data arrives.      |

## Code Walkthrough

### 1. Setup & Serial Connection

```java
size(1000, 800, P3D);
port = new Serial(this, "COM4", 115200);
```

- **`size(..., P3D)`**: Tells Processing to use the 3D rendering engine (OpenGL).
- **`port.bufferUntil('\n')`**: Optimization that waits for a full line of text before triggering an event.

### 2. Rendering the Robot (Hierarchical Transformations)

The robot is drawn using a **Stack** method. Each part's position is relative to the previous part.

1.  **`pushMatrix()`**: "Saves" the current coordinate system.
2.  **`rotateY(radians(-smoothYaw))`**: Rotates the _entire world_ for the Base.
3.  **`box(...)`**: Draws the Base.
4.  **`translate(0, -height, 0)`**: Moves the "cursor" to the top of the Base.
5.  ... repeats for Shoulder, Elbow, and Claw.
6.  **`popMatrix()`**: "Restores" the saved coordinate system.

### 3. Data Smoothing (`lerp`)

```java
smoothRoll = lerp(smoothRoll, roll, 0.15);
```

- **Linear Interpolation**: Instead of jumping instantly to the new angle (which looks twitchy), it moves 15% of the way there every frame.
