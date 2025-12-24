# CamTracker Documentation

[< Back to Project Root](../README.md)

## Overview

This Python script serves as the "eyes" of the robot. It captures video from a webcam, tracks the user's hand in real-time using Google MediaPipe, translates hand movements into servo angles, and sends them to the robot via Serial (UART).

## Dependencies

- **OpenCV (`cv2`)**: Used for image capture, processing, and display.
- **MediaPipe (`mediapipe`)**: Provides the Machine Learning model for hand landmark detection.
- **PySerial (`serial`)**: Handles communication with the robot controller (Tiva C).
- **Math (`math`)**: Used for geometry calculations (e.g., pinch distance).

## Command Reference

| Function                        | Library   | Description                                      |
| :------------------------------ | :-------- | :----------------------------------------------- |
| `cv2.VideoCapture(0)`           | OpenCV    | Opens the default webcam.                        |
| `mp.solutions.hands.Hands(...)` | MediaPipe | Initializes the Hand Tracking model.             |
| `cap.read()`                    | OpenCV    | Captures a single frame from the camera.         |
| `cv2.flip(image, 1)`            | OpenCV    | Mirrors the image horizontally.                  |
| `hands.process(image_rgb)`      | MediaPipe | Detecting 21 landmark 3D coordinates.            |
| `serial.Serial(port, baud)`     | PySerial  | Opens the COM port connection.                   |
| `ser.write(bytes)`              | PySerial  | Sends data to the robot.                         |
| `math.hypot(x, y)`              | Python    | Calculates Euclidean distance (pinch detection). |

## Code Walkthrough

### 1. Setup & Configuration

```python
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(max_num_hands=1, min_detection_confidence=0.7)
```

- **`mp.solutions.hands`**: Initializes the MediaPipe Hands module.
- **`max_num_hands=1`**: Limits detection to one hand to prevent confusion.
- **`min_detection_confidence=0.7`**: Sets a high threshold to ignore false positives (noise).

### 2. Video Capture Loop

```python
cap = cv2.VideoCapture(0)
while cap.isOpened():
    success, image = cap.read()
    image = cv2.flip(image, 1)
```

- **`cv2.VideoCapture(0)`**: Opens the default webcam (Index 0).
- **`cv2.flip(image, 1)`**: Mirrors the image horizontally so moving your hand right moves the robot right (intuitive interaction).

### 3. Logic & Control Mapping

The script maps specific landmarks to servo motors:

- **Base (X-Axis)**: Uses `wrist.x`.
  - `if wrist.x > 0.6`: Hand is on the right -> **Increment Base Angle**.
  - `if wrist.x < 0.4`: Hand is on the left -> **Decrement Base Angle**.
- **Shoulder (Y-Axis)**: Uses `index_tip.y`.
  - `if index_tip.y < 0.4`: Hand is high -> **Raise Shoulder**.
- **Elbow**: Uses relative position of `middle_tip.y` vs `wrist.y`.
  - Allows independent elbow control by tilting the hand up/down.
- **Gripper**: Uses Euclidean distance between Thumb and Index finger.
  - `math.hypot(...)`: Calculates the distance.
  - `if dist < 0.05`: **Close Gripper**.
