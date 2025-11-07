#ifndef __LED_CONTROLLER_H__
#define __LED_CONTROLLER_H__

#include "mcp_server.h"
#include "driver/mcpwm.h"
#include "esp_log.h"

#define SERVO_MIN_PULSEWIDTH_US 500  // Microsegundos para posición 0 grados
#define SERVO_MAX_PULSEWIDTH_US 2500 // Microsegundos para posición 180 grados
#define SERVO_FREQ_HZ    50   // 50Hz -> periodo de 20ms


class LedController {
private:
    bool power_ = false;
    gpio_num_t gpio_num_;

public:
    LedController(gpio_num_t gpio_num) : gpio_num_(gpio_num) {
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << gpio_num_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
        gpio_set_level(gpio_num_, 0);

        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.led.get_state", "Get the power state of the led", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return power_ ? "{\"power\": true}" : "{\"power\": false}";
        });

        mcp_server.AddTool("self.led.turn_on", "Turn on the led", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            power_ = true;
            gpio_set_level(gpio_num_, 1);
            return true;
        });

        mcp_server.AddTool("self.led.turn_off", "Turn off the led", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            power_ = false;
            gpio_set_level(gpio_num_, 0);
            return true;
        });
    }
};

class ServoController {
private:
    gpio_num_t gpio_num_;
    float current_angle_ = 90.0f;  // Iniciamos en el centro

    static inline uint32_t angle_to_pulsewidth(float angle) {
        return (angle * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / 180) + SERVO_MIN_PULSEWIDTH_US;
    }

public:
    ServoController(gpio_num_t gpio_num) : gpio_num_(gpio_num) {
        // Configurar MCPWM
        mcpwm_config_t pwm_config = {
            .frequency = SERVO_FREQ_HZ,
            .cmpr_a = 0,     // Duty cycle del PWM A
            .cmpr_b = 0,     // Duty cycle del PWM B
            .duty_mode = MCPWM_DUTY_MODE_0,
            .counter_mode = MCPWM_UP_COUNTER,
        };
        
        // Configurar el módulo MCPWM
        ESP_ERROR_CHECK(mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, gpio_num));
        ESP_ERROR_CHECK(mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config));
        
        // Mover el servo a la posición inicial (90 grados)
        setAngle(90);

        // Configurar comandos MCP
        auto& mcp_server = McpServer::GetInstance();
        
        mcp_server.AddTool("self.servo.get_angle", "Get the current angle of the servo", PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                return "{\"angle\": " + std::to_string(current_angle_) + "}";
            });

        std::vector<Property> angle_props = {
            Property("angle", kPropertyTypeInteger, 0, 180)
        };
        
        mcp_server.AddTool("self.servo.set_angle", "Set the servo angle (0-180)", PropertyList(angle_props),
            [this](const PropertyList& properties) -> ReturnValue {
                int angle = properties["angle"].value<int>();
                setAngle(angle);
                return true;
            });
    }

    void setAngle(float angle) {
        if (angle < 0) angle = 0;
        if (angle > 180) angle = 180;
        
        current_angle_ = angle;
        uint32_t pulse_width = angle_to_pulsewidth(angle);
        ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pulse_width));
    }
};

#endif // __LED_CONTROLLER_H__
