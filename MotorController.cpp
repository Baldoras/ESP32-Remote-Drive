#include "include/MotorController.h"
#include "include/LogHandler.h"
#include "include/setupConf.h"

extern LogHandler logger;

MotorController::MotorController()
    : pinEnA(0), pinIn1(0), pinIn2(0),
      pinEnB(0), pinIn3(0), pinIn4(0),
      enabled(false) {
    
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
    
    logger.info("MotorController", "Motor controller initialized");
}

void MotorController::processMovementInput(int8_t joystickX, int8_t joystickY) {
    if (!enabled) {
        stop();
        return;
    }

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
    // leftSpeed: Vorwärts/Rückwärts + Drehung nach rechts erhöht links
    // rightSpeed: Vorwärts/Rückwärts - Drehung nach rechts verringert rechts
    float leftSpeed = scaledY - scaledX;   // Range: -200 bis +200
    float rightSpeed = scaledY + scaledX;  // Range: -200 bis +200

    // Skaliere auf PWM-Bereich (0-255) basierend auf kombinierter Geschwindigkeit
    // leftSpeed/rightSpeed liegen zwischen -200 und +200
    int leftPWM = (int)(abs(leftSpeed) * 255.0 / 100.0);
    int rightPWM = (int)(abs(rightSpeed) * 255.0 / 100.0);

    // Begrenze PWM-Werte
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
    // Placeholder for periodic tasks (e.g., watchdog, safety checks)
    // Could implement timeout safety stop if no command received
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
        Serial.printf("Motor links: PWM %d direction %d\n", pwm, forward);
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
        Serial.printf("Motor rechts: PWM %d direction %d\n", pwm, forward);
        analogWrite(pinEnB, pwm);
    }
}