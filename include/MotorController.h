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
    
    // Main control method: x=steering (-100 to +100), y=throttle (-100 to +100), pwm=max speed (0-255)
    void setMovement(int8_t x, int8_t y, uint8_t pwm);
    
    // Direct motor control
    void setMotors(int8_t leftSpeed, int8_t rightSpeed, uint8_t maxPWM);
    
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
    void setLeftMotor(int8_t speed, uint8_t maxPWM);
    void setRightMotor(int8_t speed, uint8_t maxPWM);
    
    // Calculate differential steering
    void calculateDifferentialSteering(int8_t x, int8_t y, int8_t& leftSpeed, int8_t& rightSpeed);
};

#endif // MOTOR_CONTROLLER_H