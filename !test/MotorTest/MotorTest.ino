#include "BluetoothSerial.h"

//---------------------- PIN ----------------------
// Motor
#define	IO_PIN_MOTOR_1			(14)
#define	IO_PIN_MOTOR_2			(12)
#define	IO_PIN_MOTOR_3			(13)
#define	IO_PIN_MOTOR_4			(23)
#define	IO_PIN_MOTOR_ENA		(16)
#define	IO_PIN_MOTOR_ENB		(27)

//---------------------- DAC channel ----------------------
#define DAC_CH_MOTOR_A			(0)
#define DAC_CH_MOTOR_B			(1)

//---------------------- Motor ----------------------
// Direction
#define	MOTOR_DIR_STOP				(0)
#define	MOTOR_DIR_FWD				(1)
#define	MOTOR_DIR_REV				(2)


BluetoothSerial SerialBT;

int g_motor_speed = 180;

void MOTOR_init()
{
    // Initialize I/O pin
	pinMode(IO_PIN_MOTOR_1, OUTPUT);
	pinMode(IO_PIN_MOTOR_2, OUTPUT);
	pinMode(IO_PIN_MOTOR_3, OUTPUT);
	pinMode(IO_PIN_MOTOR_4, OUTPUT);
	pinMode(IO_PIN_MOTOR_ENA, OUTPUT);
	pinMode(IO_PIN_MOTOR_ENB, OUTPUT);

	// Assign the pins to D/A cnvertor channels
	ledcSetup(DAC_CH_MOTOR_A, 980, 8);
	ledcSetup(DAC_CH_MOTOR_B, 980, 8);
	ledcAttachPin(IO_PIN_MOTOR_ENA, DAC_CH_MOTOR_A);
	ledcAttachPin(IO_PIN_MOTOR_ENB, DAC_CH_MOTOR_B);
}

void MOTOR_set_speed_left(int dir, int speed)
{
	ledcWrite(DAC_CH_MOTOR_A, speed);  

	if(dir == MOTOR_DIR_FWD) {
		digitalWrite(IO_PIN_MOTOR_1,HIGH);
		digitalWrite(IO_PIN_MOTOR_2,LOW);
	}
	else if(dir == MOTOR_DIR_REV) {
		digitalWrite(IO_PIN_MOTOR_1,LOW);
		digitalWrite(IO_PIN_MOTOR_2,HIGH);
	}
	Serial.print("MOTOR_set_speed_left:");
	Serial.println(speed);
}

void MOTOR_set_speed_right(int dir, int speed)
{
	ledcWrite(DAC_CH_MOTOR_B, speed);  

	if(dir == MOTOR_DIR_FWD) {
		digitalWrite(IO_PIN_MOTOR_3,LOW);
		digitalWrite(IO_PIN_MOTOR_4,HIGH);
	}
	else if(dir == MOTOR_DIR_REV) {
		digitalWrite(IO_PIN_MOTOR_3,HIGH);
		digitalWrite(IO_PIN_MOTOR_4,LOW);
	}
	Serial.print("MOTOR_set_speed_right:");
	Serial.println(speed);
}

void RoboCar_move_forward(int speed)
{
	MOTOR_set_speed_right(MOTOR_DIR_FWD, speed);
	MOTOR_set_speed_left(MOTOR_DIR_FWD, speed);
}

void RoboCar_move_backward(int speed)
{
	MOTOR_set_speed_right(MOTOR_DIR_REV, speed);
	MOTOR_set_speed_left(MOTOR_DIR_REV, speed);
}

void RoboCar_turn_left(int dir, int speed, int level)
{
	if(level < 0) {
		level = 0;
	}
	int speed_right = speed;
	int speed_left = speed - level;
	if(speed_left < 0) {
		speed_left = 0;
	}
		
	MOTOR_set_speed_right(dir, speed_right);
	MOTOR_set_speed_left(dir, speed_left);
}

void RoboCar_turn_right(int dir, int speed, int level)
{
	if(level < 0) {
		level = 0;
	}
	int speed_right = speed - level;
	int speed_left = speed;
	if(speed_right < 0) {
		speed_right = 0;
	}
	MOTOR_set_speed_right(dir, speed_right);
	MOTOR_set_speed_left(dir, speed_left);
}

void RoboCar_rotate_left(int speed)
{
	MOTOR_set_speed_right(MOTOR_DIR_FWD, speed);
	MOTOR_set_speed_left(MOTOR_DIR_REV, speed);
}

void RoboCar_rotate_right(int speed)
{
	MOTOR_set_speed_right(MOTOR_DIR_REV, speed);
	MOTOR_set_speed_left(MOTOR_DIR_FWD, speed);
}

void RoboCar_stop()
{
	MOTOR_set_speed_left(0, 0);
	MOTOR_set_speed_right(0, 0);
}

void setup() 
{
    // Initialize serial-port (115200bps)
    Serial.begin(115200);

    // Initialize Bluetooth
	SerialBT.begin("Kubo3rd");

    // Initialize motor
    MOTOR_init();
}

void loop()
{
    if(SerialBT.available() > 0) { // received data
        int getstr = SerialBT.read(); // Read data from serial-port
        
        if(getstr == 's') {
        	MOTOR_set_speed_left(MOTOR_DIR_FWD, 0);
        	MOTOR_set_speed_right(MOTOR_DIR_REV, 0);
        }
        else if(getstr == '1') { // Left, Forward
        	MOTOR_set_speed_left(MOTOR_DIR_FWD, g_motor_speed);
        }
        else if(getstr == '2') { // Left, Backward
        	MOTOR_set_speed_left(MOTOR_DIR_REV, g_motor_speed);
        }
        else if(getstr == '3') { // Right, Forward
        	MOTOR_set_speed_right(MOTOR_DIR_FWD, g_motor_speed);
        }
        else if(getstr == '4') { // Right, Backward
        	MOTOR_set_speed_right(MOTOR_DIR_REV, g_motor_speed);
        }
        else if(getstr == 'f') { // Move forward
            RoboCar_move_forward(g_motor_speed);
        }
        else if(getstr == 'b') { // Move backward
            RoboCar_move_backward(g_motor_speed);
        }
        else if(getstr == 'l') { // Turn left
            RoboCar_turn_left(MOTOR_DIR_FWD, g_motor_speed, 60);
        }
        else if(getstr == 'r') { // Turn right
            RoboCar_turn_right(MOTOR_DIR_FWD, g_motor_speed, 60);
        }
        else if(getstr == 'L') { // Rotate left (counter clock-wise)
			RoboCar_rotate_left(g_motor_speed);
        }
        else if(getstr == 'R') { // Rotate right (clock-wise)
			RoboCar_rotate_right(g_motor_speed);
        }
    }
}
