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
    logger.log(LogLevel::LOG_INFO, LogCategory::LOG_CAT_GENERAL, "MotorController", "Initializing motor controller");
    
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
    
    // Set PWM frequency (optional, ESP32 default is ~5kHz)
    // ledcSetup() can be used here if needed
    
    // Initial state: motors stopped
    stop();
    
    logger->log(LogLevel::LOG_INFO, LogCategory::LOG_CAT_GENERAL, "MotorController", "Motor controller initialized");
}

void MotorController::setMovement(int8_t x, int8_t y, uint8_t pwm) {
    if (!enabled) {
        logger->log(LogLevel::LOG_WARNING, LogCategory::LOG_CAT_GENERAL,"MotorController", "Movement command ignored - motors disabled");
        return;
    }
    
    // Constrain inputs
    x = constrain(x, -100, 100);
    y = constrain(y, -100, 100);
    pwm = constrain(pwm, 0, 255);
    
    // Calculate differential steering
    int8_t leftSpeed, rightSpeed;
    calculateDifferentialSteering(x, y, leftSpeed, rightSpeed);
    
    // Apply motor speeds
    setMotors(leftSpeed, rightSpeed, pwm);
    
    logger.log(LogLevel::LOG_DEBUG, "MotorController", 
               String("Movement: x=") + x + " y=" + y + " pwm=" + pwm + 
               " -> L=" + leftSpeed + " R=" + rightSpeed);
}

void MotorController::setMotors(int8_t leftSpeed, int8_t rightSpeed, uint8_t maxPWM) {
    leftSpeed = constrain(leftSpeed, -100, 100);
    rightSpeed = constrain(rightSpeed, -100, 100);
    
    setLeftMotor(leftSpeed, maxPWM);
    setRightMotor(rightSpeed, maxPWM);
    
    // Update telemetry
    telemetry.leftSpeed = leftSpeed;
    telemetry.rightSpeed = rightSpeed;
    telemetry.leftPWM = map(abs(leftSpeed), 0, 100, 0, maxPWM);
    telemetry.rightPWM = map(abs(rightSpeed), 0, 100, 0, maxPWM);
    telemetry.lastUpdateMs = millis();
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
    
    logger.log(LogLevel::LOG_INFO, LogCategory::LOG_CAT_GENERAL,"MotorController", "Motors stopped");
}

void MotorController::enable() {
    enabled = true;
    telemetry.motorsEnabled = true;
    logger.log(LogLevel::LOG_INFO, LogCategory::LOG_CAT_GENERAL,"MotorController", "Motors enabled");
}

void MotorController::disable() {
    enabled = false;
    telemetry.motorsEnabled = false;
    stop();
    logger.log(LogLevel::LOG_INFO, LogCategory::LOG_CAT_GENERAL,"MotorController", "Motors disabled");
}

MotorTelemetry MotorController::getTelemetry() const {
    return telemetry;
}

void MotorController::update() {
    // Placeholder for periodic tasks (e.g., watchdog, safety checks)
    // Could implement timeout safety stop if no command received
}

void MotorController::setLeftMotor(int8_t speed, uint8_t maxPWM) {
    uint8_t pwmValue = map(abs(speed), 0, 100, 0, maxPWM);
    
    if (speed > 0) {
        // Forward
        digitalWrite(pinIn1, HIGH);
        digitalWrite(pinIn2, LOW);
    } else if (speed < 0) {
        // Backward
        digitalWrite(pinIn1, LOW);
        digitalWrite(pinIn2, HIGH);
    } else {
        // Stop
        digitalWrite(pinIn1, LOW);
        digitalWrite(pinIn2, LOW);
    }
    
    analogWrite(pinEnA, pwmValue);
}

void MotorController::setRightMotor(int8_t speed, uint8_t maxPWM) {
    uint8_t pwmValue = map(abs(speed), 0, 100, 0, maxPWM);
    
    if (speed > 0) {
        // Forward
        digitalWrite(pinIn3, HIGH);
        digitalWrite(pinIn4, LOW);
    } else if (speed < 0) {
        // Backward
        digitalWrite(pinIn3, LOW);
        digitalWrite(pinIn4, HIGH);
    } else {
        // Stop
        digitalWrite(pinIn3, LOW);
        digitalWrite(pinIn4, LOW);
    }
    
    analogWrite(pinEnB, pwmValue);
}

void MotorController::calculateDifferentialSteering(int8_t x, int8_t y, int8_t& leftSpeed, int8_t& rightSpeed) {
    // Differential steering algorithm
    // x: steering (-100 left, +100 right)
    // y: throttle (-100 backward, +100 forward)
    
    // Basic mixing algorithm
    float left = y + x;
    float right = y - x;
    
    // Normalize if values exceed range
    float maxMagnitude = max(abs(left), abs(right));
    if (maxMagnitude > 100.0) {
        left = (left / maxMagnitude) * 100.0;
        right = (right / maxMagnitude) * 100.0;
    }
    
    leftSpeed = (int8_t)constrain(left, -100, 100);
    rightSpeed = (int8_t)constrain(right, -100, 100);
}