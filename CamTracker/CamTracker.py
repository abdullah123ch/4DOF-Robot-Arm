import cv2
import mediapipe as mp
import math
import serial
import time

# --- CONFIGURATION ---
USE_SERIAL = True
SERIAL_PORT = 'COM17' 
BAUD_RATE = 9600

# Servo Limits & Speed
MIN_ANGLE = 0
MAX_ANGLE = 180
MOVE_SPEED = 2  # How fast the servos move per frame (Increase for faster robot)
DEADZONE = 0.1  # 10% center area where robot doesn't move (Resting Zone)

# Initial Servo Positions (Start at 90 to be safe)
current_base = 90
current_shoulder = 90
current_elbow = 90
current_gripper = 0

# Initialize Serial
if USE_SERIAL:
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
    except Exception as e:
        print(f"Serial Error: {e}")
        USE_SERIAL = False

# MediaPipe Setup
mp_hands = mp.solutions.hands
mp_drawing = mp.solutions.drawing_utils
hands = mp_hands.Hands(max_num_hands=1, min_detection_confidence=0.7)

cap = cv2.VideoCapture(0)

print("--- CONTROLS ---")
print("Move Hand from Center -> Robot Moves")
print("Return Hand to Center -> Robot Stops (Resting)")
print("Press 'r' -> Reset all servos to 0")
print("Press 'q' -> Quit")

while cap.isOpened():
    success, image = cap.read()
    if not success: continue

    # Flip for mirror view
    image = cv2.flip(image, 1)
    h, w, _ = image.shape
    
    image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
    results = hands.process(image_rgb)

    # Draw "Resting Zone" Box in Center
    cv2.rectangle(image, (int(w*0.4), int(h*0.4)), (int(w*0.6), int(h*0.6)), (0, 255, 0), 2)
    cv2.putText(image, "RESTING ZONE", (int(w*0.42), int(h*0.38)), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

    key = cv2.waitKey(5) & 0xFF

    # --- RESET LOGIC (R Key) ---
    if key == ord('r'):
        current_base = 90
        current_shoulder = 90
        current_elbow = 90
        current_gripper = 0
        cv2.putText(image, "RESETTING...", (50, 250), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 3)

    elif results.multi_hand_landmarks:
        for hand_landmarks in results.multi_hand_landmarks:
            mp_drawing.draw_landmarks(image, hand_landmarks, mp_hands.HAND_CONNECTIONS)
            
            # Get Key Landmarks
            wrist = hand_landmarks.landmark[mp_hands.HandLandmark.WRIST]
            index_tip = hand_landmarks.landmark[mp_hands.HandLandmark.INDEX_FINGER_TIP]
            thumb_tip = hand_landmarks.landmark[mp_hands.HandLandmark.THUMB_TIP]
            middle_tip = hand_landmarks.landmark[mp_hands.HandLandmark.MIDDLE_FINGER_TIP]

            # --- VELOCITY CONTROL LOGIC ---
            
            # 1. BASE (Left/Right)
            # Center of screen is 0.5. 
            # If wrist.x > 0.6 (Right), increase angle. If < 0.4 (Left), decrease.
            if wrist.x > (0.5 + DEADZONE):     # Hand Right
                current_base += MOVE_SPEED
            elif wrist.x < (0.5 - DEADZONE):   # Hand Left
                current_base -= MOVE_SPEED
            
            # 2. SHOULDER (Up/Down) - Based on Index Finger Height
            # Screen Y is 0 at top, 1 at bottom.
            if index_tip.y < (0.5 - DEADZONE): # Hand High (Top)
                current_shoulder += MOVE_SPEED
            elif index_tip.y > (0.5 + DEADZONE): # Hand Low (Bottom)
                current_shoulder -= MOVE_SPEED

            # 3. ELBOW (Up/Down) - Based on Middle Finger Height (Relative to Wrist)
            # If Middle finger is significantly higher than wrist -> Extend
            # This allows independent elbow control if you tilt your hand
            if middle_tip.y < wrist.y - 0.1:  # Finger pointing up
                current_elbow += MOVE_SPEED
            elif middle_tip.y > wrist.y + 0.1: # Finger pointing down
                current_elbow -= MOVE_SPEED

            # 4. GRIPPER (Pinch) - Absolute control is better here
            pinch_dist = math.hypot(index_tip.x - thumb_tip.x, index_tip.y - thumb_tip.y)
            if pinch_dist < 0.05:
                current_gripper = 0   # Close
                cv2.putText(image, "GRIP: CLOSED", (10, 150), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            else:
                current_gripper = 180 # Open

    # --- CLAMP VALUES (Safety) ---
    current_base = max(MIN_ANGLE, min(MAX_ANGLE, current_base))
    current_shoulder = max(MIN_ANGLE, min(MAX_ANGLE, current_shoulder))
    current_elbow = max(MIN_ANGLE, min(MAX_ANGLE, current_elbow))

    # --- DATA OUTPUT ---
    # Convert floats to integers for string formatting
    packet = f"B{int(current_base)}S{int(current_shoulder)}E{int(current_elbow)}G{int(current_gripper)}\n"
    
    if USE_SERIAL:
        ser.write(packet.encode('utf-8'))

    # Display Status
    cv2.putText(image, f"Base: {int(current_base)}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
    cv2.putText(image, f"Shoulder: {int(current_shoulder)}", (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
    cv2.putText(image, f"Elbow: {int(current_elbow)}", (10, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)

    cv2.imshow('Hand Control', image)
    if key == ord('q'):
        break

cap.release()
if USE_SERIAL: ser.close()
cv2.destroyAllWindows()