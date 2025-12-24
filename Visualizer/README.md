# Visualizer Documentation (Processing)

## Overview

This application creates a **Digital Twin** of the robot arm. It runs on your computer and visualizes the robot's movements in real-time 3D. It is useful for verifying that the math and sensor data are correct before turning on the actual high-power motors.

## Dependencies

- **Processing 3 or 4**: The IDE and runtime environment.
- **processing.serial**: Library to read data from the USB port.

## Code Walkthrough & Command Explanation

### 1. Setup & Serial Connection

```java
size(1000, 800, P3D);
port = new Serial(this, "COM4", 115200);
```

- **`size(..., P3D)`**: Tells Processing to use the 3D rendering engine (OpenGL).
- **`new Serial(...)`**: Opens the COM port to listen for data.
- **`port.bufferUntil('\n')`**: Optimization that waits for a full line of text (ending in newline) before triggering an event, rather than checking every single byte.

### 2. The Draw Loop (The 3D Scene)

Processing runs `draw()` 60 times a second.

- **`background(30, 30, 40)`**: Clears the screen with a dark grey color.
- **`lights()`**: Enables default lighting so the 3D boxes look like valid shapes (shading).
- **`camera(...) / translate(...)`**: Positions the virtual "camera" to look at the robot.

### 3. Rendering the Robot (Hierarchical Transformations)

The robot is drawn using a **Stack** method. Each part's position is relative to the previous part.

1.  **`pushMatrix()`**: "Saves" the current coordinate system.
2.  **`rotateY(radians(-smoothYaw))`**: Rotates the _entire world_ for the Base.
3.  **`box(...)`**: Draws the Base.
4.  **`translate(0, -height, 0)`**: Moves the "cursor" to the top of the Base.
5.  **`rotateZ(...)`**: Rotates the coordinate system for the Shoulder.
6.  **`box(...)`**: Draws the Arm.
7.  ... repeats for Elbow and Claw.
8.  **`popMatrix()`**: "Restores" the saved coordinate system. This ensures that when we finish the Base, we go back to the world origin.

### 4. Data Smoothing (`lerp`)

```java
smoothRoll = lerp(smoothRoll, roll, 0.15);
```

- **Linear Interpolation**: Instead of jumping instantly to the new angle (which looks twitchy), it moves 15% of the way there every frame.
- This creates a smooth, fluid animation that mimics the physical inertia of real motors.

### 5. Data Parsing (`serialEvent`)

- Reads the string: `$E,10,45.5,90.2,0` (Example).
- **`split(data, ',')`**: Breaks it into an array: `["$E", "10", "45.5", "90.2", "0"]`.
- **`float()`**: Converts text to numbers.
