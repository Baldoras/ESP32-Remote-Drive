#include "include/MotorController.h"
#include "include/LogHandler.h"
#include "include/setupConf.h"

extern LogHandler logger;

MotorController::MotorController()
    : pinEnA(0), pinIn1(0), pinIn2(0),
      pinEnB(0), pinIn3(0), pinIn4(0),
      enabled(false),
      lastCommandTime(0) {
    
    // Initialize telemetry
    telemetry.leftSpeed = 0;
    telemetry.rightSpeed = 0;
    telemetry.leftPWM = 0;
    telemetry.rightPWM = 0;
    telemetry.motorsEnabled = false;
    telemetry.lastUpdateMs = 0;
}

void MotorController::begin() {
    logger.info("MotorController", "Initializing motor controller");
    
    // Read pin configuration from setupConf.h
    pinEnA = MOTOR_ENA;
    pinIn1 = MOTOR_IN1;
    pinIn2 = MOTOR_IN2;
    pinEnB = MOTOR_ENB;
    pinIn3 = MOTOR_IN3;
    pinIn4 = MOTOR_IN4;
    
    // Configure pins
    pinMode(pinEnA, OUTPUT);
    pinMode(pinIn1, OUTPUT);
    pinMode(pinIn2, OUTPUT);
    pinMode(pinEnB, OUTPUT);
    pinMode(pinIn3, OUTPUT);
    pinMode(pinIn4, OUTPUT);
    
    // Initial state: motors stopped
    stop();
    
    // Initialize safety timeout
    lastCommandTime = millis();
    
    logger.info("MotorController", "Motor controller initialized");
}

void MotorController::processMovementInput(int8_t joystickX, int8_t joystickY) {
    if (!enabled) {
        stop();
        return;
    }

    // ═══════════════════════════════════════════════════════════════════
    // SAFETY: Command empfangen - Timer zurücksetzen
    // ═══════════════════════════════════════════════════════════════════
    lastCommandTime = millis();

    // Berechne Abstand vom Nullpunkt (Joystick-Auslenkung)
    float distance = sqrt(joystickX * joystickX + joystickY * joystickY);
    
    // Skaliere auf maximal 100, falls Abstand größer
    float scaleFactor = 1.0;
    if (distance > 100.0) {
        scaleFactor = 100.0 / distance;
    }
    
    // Skalierte Joystick-Werte
    float scaledX = joystickX * scaleFactor;
    float scaledY = joystickY * scaleFactor;

    // Differential steering
    float leftSpeed = scaledY - scaledX;   // Range: -100 bis +100
    float rightSpeed = scaledY + scaledX;  // Range: -100 bis +100

    // ═══════════════════════════════════════════════════════════════════
    // PWM-Skalierung: 127-255 (127 = Minimum, 255 = Maximum)
    // ═══════════════════════════════════════════════════════════════════
    
    int leftPWM = 0;
    int rightPWM = 0;
    
    if (abs(leftSpeed) > 0) {
        // Map von 0-100 auf 127-255
        leftPWM = map((int)abs(leftSpeed), 0, 100, 127, 255);
    }
    
    if (abs(rightSpeed) > 0) {
        // Map von 0-100 auf 127-255
        rightPWM = map((int)abs(rightSpeed), 0, 100, 127, 255);
    }
    
    // Begrenze PWM-Werte zur Sicherheit
    leftPWM = constrain(leftPWM, 0, 255);
    rightPWM = constrain(rightPWM, 0, 255);

    // Setze Motoren (Richtung und PWM)
    setMotor(MOTOR_ID_LEFT, leftSpeed >= 0, leftPWM);
    setMotor(MOTOR_ID_RIGHT, rightSpeed >= 0, rightPWM);
    
    // Update telemetry
    telemetry.leftSpeed = (int8_t)constrain(leftSpeed, -100, 100);
    telemetry.rightSpeed = (int8_t)constrain(rightSpeed, -100, 100);
    telemetry.leftPWM = leftPWM;
    telemetry.rightPWM = rightPWM;
    telemetry.lastUpdateMs = millis();
    
    // Debug logging
    logger.debug("MotorController",
               (String("Movement: X=") + joystickX + " Y=" + joystickY + 
               " -> L=" + leftSpeed + "(" + leftPWM + ") R=" + rightSpeed + "(" + rightPWM + ")").c_str());
}

void MotorController::stop() {
    // Set all motor pins low
    digitalWrite(pinIn1, LOW);
    digitalWrite(pinIn2, LOW);
    digitalWrite(pinIn3, LOW);
    digitalWrite(pinIn4, LOW);
    analogWrite(pinEnA, 0);
    analogWrite(pinEnB, 0);
    
    // Update telemetry
    telemetry.leftSpeed = 0;
    telemetry.rightSpeed = 0;
    telemetry.leftPWM = 0;
    telemetry.rightPWM = 0;
    telemetry.lastUpdateMs = millis();
    
    logger.info("MotorController", "Motors stopped");
}

void MotorController::enable() {
    enabled = true;
    telemetry.motorsEnabled = true;
    lastCommandTime = millis();  // Reset timeout bei Enable
    logger.info("MotorController", "Motors enabled");
}

void MotorController::disable() {
    enabled = false;
    telemetry.motorsEnabled = false;
    stop();
    logger.info("MotorController", "Motors disabled");
}

MotorTelemetry MotorController::getTelemetry() const {
    return telemetry;
}

void MotorController::update() {
    // ═══════════════════════════════════════════════════════════════════
    // SAFETY CHECK: Command Timeout
    // ═══════════════════════════════════════════════════════════════════
    checkCommandTimeout();
}

void MotorController::checkCommandTimeout() {
    // Nur prüfen wenn Motoren enabled sind
    if (!enabled) {
        return;
    }
    
    // Prüfe ob Motoren aktuell laufen (PWM > 0)
    bool motorsRunning = (telemetry.leftPWM > 0 || telemetry.rightPWM > 0);
    
    if (!motorsRunning) {
        return;  // Motoren sind bereits aus, nichts zu tun
    }
    
    // Prüfe Timeout
    unsigned long timeSinceLastCommand = millis() - lastCommandTime;
    
    if (timeSinceLastCommand > COMMAND_TIMEOUT_MS) {
        // TIMEOUT! Emergency Stop
        Serial.println("\n╔════════════════════════════════════════╗");
        Serial.println("║  ⚠️  SAFETY TIMEOUT - EMERGENCY STOP  ║");
        Serial.println("╚════════════════════════════════════════╝");
        Serial.printf("Time since last command: %lu ms (limit: %lu ms)\n", 
                     timeSinceLastCommand, COMMAND_TIMEOUT_MS);
        Serial.printf("Motors were running: L=%d, R=%d\n", 
                     telemetry.leftPWM, telemetry.rightPWM);
        
        logger.warning("MotorController", "Command timeout - emergency stop!");
        
        // STOP!
        stop();
        
        Serial.println("✅ Motors stopped for safety");
        Serial.println("════════════════════════════════════════\n");
    }
}

void MotorController::setMotor(uint8_t motor, bool forward, uint8_t pwm) {
    if (motor == MOTOR_ID_LEFT) {
        // Left motor control
        if (forward) {
            digitalWrite(pinIn1, HIGH);
            digitalWrite(pinIn2, LOW);
        } else {
            digitalWrite(pinIn1, LOW);
            digitalWrite(pinIn2, HIGH);
        }
        analogWrite(pinEnA, pwm);
    } 
    else if (motor == MOTOR_ID_RIGHT) {
        // Right motor control
        if (forward) {
            digitalWrite(pinIn3, HIGH);
            digitalWrite(pinIn4, LOW);
        } else {
            digitalWrite(pinIn3, LOW);
            digitalWrite(pinIn4, HIGH);
        }
        analogWrite(pinEnB, pwm);
    }
}