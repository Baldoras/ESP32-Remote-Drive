#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>
#include "setupConf.h"

// Motor telemetry data structure
struct MotorTelemetry {
    int8_t leftSpeed;      // -100 to +100
    int8_t rightSpeed;     // -100 to +100
    uint8_t leftPWM;       // 0-255
    uint8_t rightPWM;      // 0-255
    bool motorsEnabled;
    uint32_t lastUpdateMs;
};

class MotorController {
public:
    // Constructor
    MotorController();
    
    // Initialize motor controller
    void begin();
    
    // Process movement input with improved differential steering
    void processMovementInput(int8_t joystickX, int8_t joystickY);
    
    // Emergency stop
    void stop();
    
    // Enable/disable motors
    void enable();
    void disable();
    
    // Get telemetry data
    MotorTelemetry getTelemetry() const;
    
    // Update method for periodic tasks
    void update();

private:
    // Pin definitions
    uint8_t pinEnA, pinIn1, pinIn2;  // Left motor
    uint8_t pinEnB, pinIn3, pinIn4;  // Right motor
    
    // Current state
    MotorTelemetry telemetry;
    bool enabled;
    
    // Internal motor control
    void setMotor(uint8_t motor, bool forward, uint8_t pwm);
};

#endif // MOTOR_CONTROLLER_H