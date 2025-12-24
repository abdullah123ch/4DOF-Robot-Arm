# CamTracker Documentation

## Overview

This Python script serves as the "eyes" of the robot. It captures video from a webcam, tracks the user's hand in real-time using Google MediaPipe, translates hand movements into servo angles, and sends them to the robot via Serial (UART).

## Dependencies

- **OpenCV (`cv2`)**: Used for image capture, processing, and display.
- **MediaPipe (`mediapipe`)**: Provides the Machine Learning model for hand landmark detection.
- **PySerial (`serial`)**: Handles communication with the robot controller (Tiva C).
- **Math (`math`)**: Used for geometry calculations (e.g., pinch distance).

## Code Walkthrough & Command Explanation

### 1. Setup & Configuration

```python
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(...)
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
- **`cap.read()`**: Grabs a single frame from the camera.
- **`cv2.flip(image, 1)`**: Mirrors the image horizontally so moving your hand right moves the robot right (intuitive interaction).

### 3. Hand Tracking

```python
results = hands.process(image_rgb)
```

- **`cv2.cvtColor(..., cv2.COLOR_BGR2RGB)`**: MediaPipe expects RGB images, but OpenCV uses BGR. This command converts the color space.
- **`hands.process()`**: Runs the heavy ML inference to find 21 hand landmarks.

### 4. Logic & Control Mapping

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

### 5. Data Transmission

```python
packet = f"B{int(base)}S{int(shoulder)}E{int(elbow)}G{int(gripper)}\n"
ser.write(packet.encode('utf-8'))
```

- **`f"..."`**: Formats the data into a string (e.g., "B90S90E90G0").
- **`ser.write()`**: Sends the bytes over the USB-Serial cable to the robot.

### 6. Drawing & Display

```python
mp_drawing.draw_landmarks(image, hand_landmarks, ...)
cv2.imshow('Hand Control', image)
```

- **`draw_landmarks`**: Overlays the skeleton wireframe on the hand.
- **`cv2.imshow`**: Updates the desktop window with the new frame.
