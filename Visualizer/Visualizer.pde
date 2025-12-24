import processing.serial.*;

Serial port;
float roll = 0, pitch = 0, yaw = 0;
int clawState = 0; // 0 = Open, 1 = Closed

// Smoothing variables
float smoothRoll = 0, smoothPitch = 0, smoothYaw = 0;
float smoothing = 0.15; // Lower is smoother but slower

// Robot Arm Dimensions (Looks cool on screen)
float baseHeight = 40;
float baseWidth = 80;
float armLength1 = 120; // Shoulder to Elbow
float armLength2 = 100; // Elbow to Wrist
float clawSize = 30;

void setup() {
  size(1000, 800, P3D);
  surface.setTitle("4-DOF Robot Arm Digital Twin");
  pixelDensity(1);
  
  // Auto-connect to first serial port
  try {
    String portName = Serial.list()[0];
    port = new Serial(this, "COM4", 115200);
    port.bufferUntil('\n');
    println("Connected to: " + portName);
  } catch (Exception e) {
    println("Error: No Serial Device Found. Check connection!");
  }
}

void draw() {
  background(30, 30, 40); // Dark engineering grey
  lights(); // Add lighting for 3D depth
  directionalLight(255, 255, 255, 0, 1, -1);
  
  // Smooth the data
  smoothRoll = lerp(smoothRoll, roll, smoothing);
  smoothPitch = lerp(smoothPitch, pitch, smoothing);
  smoothYaw = lerp(smoothYaw, yaw, smoothing);
  
  // Set up the camera/view
  translate(width/2, height/2 + 100, -200); // Move scene to center
  rotateX(radians(-20)); // Tilt view slightly down to see depth
  
  // Draw Floor
  drawGrid();

  // === ROBOT ARM HIERARCHY START ===
  
  // 1. BASE (Controlled by YAW)
  pushMatrix();
    // Rotate Base around Y axis (Vertical)
    // Map Yaw (-90 to 90) to rotation
    rotateY(radians(-smoothYaw)); 
    
    // Draw Base
    fill(100, 100, 100); // Grey
    noStroke();
    translate(0, -baseHeight/2, 0);
    box(baseWidth, baseHeight, baseWidth);
    
    // 2. SHOULDER (Controlled by PITCH)
    translate(0, -baseHeight/2, 0); // Move to top of base
    pushMatrix();
      rotateZ(radians(smoothPitch)); // Rotate around Z axis (Sideways tilt for shoulder)
      // Or rotateX depending on your MPU orientation. Swap if needed.
      
      // Draw Lower Arm
      fill(0, 150, 255); // Blue
      translate(0, -armLength1/2, 0); // Move up half the arm length
      box(30, armLength1, 30);
      
      // 3. ELBOW (Controlled by ROLL)
      translate(0, -armLength1/2, 0); // Move to top of lower arm
      pushMatrix();
        rotateZ(radians(smoothRoll)); // Rotate relative to shoulder
        
        // Draw Upper Arm
        fill(255, 50, 50); // Red
        translate(0, -armLength2/2, 0);
        box(25, armLength2, 25);
        
        // 4. CLAW (Controlled by Button)
        translate(0, -armLength2/2, 0); // Move to end of arm
        drawClaw(clawState);
        
      popMatrix(); // End Elbow
    popMatrix(); // End Shoulder
  popMatrix(); // End Base
  
  // Text Overlay
  drawHUD();
}

void drawClaw(int state) {
  fill(255, 255, 0); // Yellow
  
  float openWidth = 20;
  if (state == 1) openWidth = 5; // Close if button pressed
  
  // Draw Wrist block
  box(40, 10, 20);
  
  // Left Finger
  pushMatrix();
    translate(-15, -20, 0);
    translate(openWidth/2, 0, 0); // Animate closing
    box(5, 30, 10);
  popMatrix();
  
  // Right Finger
  pushMatrix();
    translate(15, -20, 0);
    translate(-openWidth/2, 0, 0); // Animate closing
    box(5, 30, 10);
  popMatrix();
}

void drawGrid() {
  stroke(255, 50);
  for(int i=-500; i<=500; i+=50) {
    line(i, 0, -500, i, 0, 500);
    line(-500, 0, i, 500, 0, i);
  }
}

void drawHUD() {
  cam.beginHUD(); // Reset matrix for text
  fill(255);
  textSize(18);
  text("ROBOT ARM TELEMETRY", 20, 30);
  text("Base (Yaw): " + nfc(smoothYaw, 1), 20, 60);
  text("Shoulder (Pitch): " + nfc(smoothPitch, 1), 20, 80);
  text("Elbow (Roll): " + nfc(smoothRoll, 1), 20, 100);
  
  String cStatus = (clawState == 1) ? "CLOSED" : "OPEN";
  fill((clawState == 1) ? #FF0000 : #00FF00);
  text("Claw: " + cStatus, 20, 130);
  cam.endHUD();
}

// Simple Camera HUD hack
class Cam {
  void beginHUD() { pushMatrix(); hint(DISABLE_DEPTH_TEST); camera(); noLights(); }
  void endHUD() { hint(ENABLE_DEPTH_TEST); popMatrix(); }
}
Cam cam = new Cam();

void serialEvent(Serial p) {
  try {
    String data = p.readStringUntil('\n');
    if (data != null) {
      data = trim(data);
      if (data.startsWith("$E")) {
        String[] parts = split(data, ',');
        if (parts.length == 5) {
          roll = float(parts[1]);
          pitch = float(parts[2]);
          yaw = float(parts[3]);
          clawState = int(parts[4]);
        }
      }
    }
  } catch(Exception e) {}
}
