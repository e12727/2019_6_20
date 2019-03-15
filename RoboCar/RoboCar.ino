#include <Wire.h>
#include <RPR-0521RS.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "BluetoothSerial.h"
#include "esp_system.h"
#include "dev_MPU6050.h"
#include "freertos/task.h"

//====================== PIN ======================
// Motor
#define	IO_PIN_MOTOR_1			(14)
#define	IO_PIN_MOTOR_2			(12)
#define	IO_PIN_MOTOR_3			(13)
#define	IO_PIN_MOTOR_4			(23)
#define	IO_PIN_MOTOR_ENA		(16)
#define	IO_PIN_MOTOR_ENB		(27)
// LED
#define	IO_PIN_LED				(18)
#define	IO_PIN_LED2				(2)
// US Sendor
#define	IO_PIN_US_ECHO			(36)
#define	IO_PIN_US_TRIG			( 5)
// Servo
#define	IO_PIN_SERVO			(25)
// �ԊO��
#define IO_PIN_INFRARED			(19)
// Line Tracking Sensor
#define IO_PIN_LINETRACK_LEFT	(26)
#define IO_PIN_LINETRACK_CENTER	(17)
#define IO_PIN_LINETRACK_RIGHT	(39)
// I2C
#define IO_PIN_SDA 				SDA
#define IO_PIN_SCL 				SCL

// DAC��channel
#define DAC_CH_MOTOR_A			(0)
#define DAC_CH_MOTOR_B			(1)
#define DAC_CH_SERVO			(2)


#define	MOTOR_RIGHT_FRONT		0x01
#define	MOTOR_RIGHT_REAR		0x02
#define	MOTOR_LEFT_FRONT		0x04
#define	MOTOR_LEFT_REAR			0x08

#define	MOTOR_RIGHT				(MOTOR_RIGHT_FRONT|MOTOR_RIGHT_REAR)
#define	MOTOR_LEFT				(MOTOR_LEFT_FRONT|MOTOR_LEFT_REAR)
#define	MOTOR_FRONT				(MOTOR_RIGHT_FRONT|MOTOR_LEFT_FRONT)
#define	MOTOR_REAR				(MOTOR_RIGHT_REAR|MOTOR_LEFT_REAR)

#define	MOTOR_DIR_STOP				(0)
#define	MOTOR_DIR_FWD				(1)
#define	MOTOR_DIR_REV				(2)

// state of motor
#define	STATE_MOTOR_STOP				(0)
#define	STATE_MOTOR_MOVING_FORWARD		(1)
#define	STATE_MOTOR_MOVING_BACKWARD		(2)
#define	STATE_MOTOR_TURNING_RIGHT		(3)
#define	STATE_MOTOR_TURNING_LEFT		(4)
#define	STATE_MOTOR_TURNING_RIGHT_BACK	(5)
#define	STATE_MOTOR_TURNING_LEFT_BACK	(6)
#define	STATE_MOTOR_ROTATING_CW			(7)
#define	STATE_MOTOR_ROTATING_CCW		(8)

#define	CTRLMODE_AUTO_DRIVE			(0)
#define	CTRLMODE_MANUAL_DRIVE		(1)
#define	CTRLMODE_LINE_TRACKING		(2)

#define	COMMAND_CODE_STOP			(0)	// �����Ȃ�
#define	COMMAND_CODE_FORWARD		(1) // �����Ȃ�
#define	COMMAND_CODE_MOTOR_SPEED	(2)	// [1] Speed

#define	MOTOR_SPEED_MIN				(40)
#define	MOTOR_SPEED_MAX				(255)

typedef struct {
	int		event;
	int		param1;
} _t_queue_event;

//BluetoothSerial SerialBT;

RPR0521RS rpr0521rs;

int		g_log_level = 3; // �\������log�̃��x��(0�`99)
float g_proficiency_score = 0.0;	// �n���x
QueueHandle_t g_xQueue_Serial;



int	g_state_motor = STATE_MOTOR_STOP;
int g_motor_speed = 170;
int g_motor_speed_on_left_turn = 230;
int g_motor_speed_on_right_turn = 230;
int g_lr_level_on_left_turn = 90;
int g_lr_level_on_right_turn = 110;
int g_motor_speed_right;
int g_motor_speed_left;
int	g_ctrl_mode = CTRLMODE_MANUAL_DRIVE;

int g_stop_distance = 20;	// [cm]

float servo_coeff_a;
float servo_coeff_b;

SemaphoreHandle_t g_xMutex = NULL;

#if 1
// Wifi �A�N�Z�X�|�C���g�̏��
const char* ssid = "SPWH_H32_F37CDD"; // WiFi���[�^1
//const char* ssid = "SPWH_H32_5AE424"; // WiFi���[�^2
const char* password = "********";
//const char* password = "19iyteirq5291f2"; // WiFi���[�^1
//const char* password = "jaffmffm04mf01i"; // WiFi���[�^2

// �����Őݒ肵�� CloudMQTT.xom �T�C�g�� Instance info ����擾
const char* mqttServer = "m16.cloudmqtt.com";
const char* mqttDeviceId = "KMCar001";
const char* mqttUser = "vsscjrry";
const char* mqttPassword = "kurgC_M_VZmF";
const int mqttPort = 17555;

// Subscribe ���� MQTT Topic ��
const char* mqttTopic_Signal = "KM/Signal";
const char* mqttTopic_Sensor = "KM/Sensor";
const char* mqttTopic_Status = "KM/Status";
const char* mqttTopic_Command = "KM/Command";
const char* mqttTopic_Param = "KM/Param";
const char* mqttTopic_Query = "KM/Query";

//Connect WiFi Client and MQTT(PubSub) Client
WiFiClient espClient;
PubSubClient client(espClient);

/* 
 *  Subscribe ���Ă��� Topic �Ƀ��b�Z�[�W���������ɏ��������� Callback �֐���ݒ�B
 *  �����ł͒P�Ƀ��b�Z�[�W�����o���Ă��邾���B
 *  JSON �`���ɂ��Ă邯�ǁA����܂�K�v�Ȃ������ł���΁ATopic �� Message �����Ŕ��ʂ��������B
 *  �ł� JSON �`���ɂ��Ă����ƁA�ォ�画�ʂ������肷��ۂɎg���₷������A�ǂ����邩�B
 */
void callback_MQTT(char* topic, byte* payload, unsigned int length) 
{
	//----- JSON�`���̃f�[�^�����o��
	StaticJsonDocument<200> doc;
	// Deserialize
	deserializeJson(doc, payload);
	// extract the data
	JsonObject object = doc.as<JsonObject>();
	if(strcmp(topic, mqttTopic_Signal) == 0) {
		const char* led = object["LED"];
		if(led != NULL) {
			if(strcmp(led, "GREEN") == 0) {
				digitalWrite(IO_PIN_LED2, HIGH);
			}
			else if(strcmp(led, "RED") == 0) {
				digitalWrite(IO_PIN_LED2, LOW);
			}
		}
	}

	if(strcmp(topic, mqttTopic_Command) == 0) {
		if(!object["Stop"].isNull()) {
			int getstr = 's';
			xQueueSend(g_xQueue_Serial, &getstr, 100);
		}
		if(!object["MotorSpeed"].isNull()) {
			int speed = object["MotorSpeed"].as<int>();
			if((MOTOR_SPEED_MIN<=speed) && (speed<=MOTOR_SPEED_MAX)) {
				g_motor_speed = speed;
			}
		}
		if(!object["MotorSpeedLR"].isNull()) {
			int left = object["MotorSpeedLR"][0].as<int>();
			int right = object["MotorSpeedLR"][1].as<int>();
			int left_l = object["MotorSpeedLR"][2].as<int>();
			int right_l = object["MotorSpeedLR"][3].as<int>();
			if((MOTOR_SPEED_MIN<=left) && (left<=MOTOR_SPEED_MAX)) {
				g_motor_speed_on_left_turn = left;
			}
			if((MOTOR_SPEED_MIN<=right) && (right<=MOTOR_SPEED_MAX)) {
				g_motor_speed_on_right_turn = right;
			}
			if((0<=left_l) && (left_l<=MOTOR_SPEED_MAX)) {
				g_lr_level_on_left_turn = left_l;
			}
			if((0<=right_l) && (right_l<=MOTOR_SPEED_MAX)) {
				g_lr_level_on_right_turn = right_l;
			}
		}
	}

	if(strcmp(topic, mqttTopic_Query) == 0) {
		if(!object["Id"].isNull()) {
			const char* code = object["Id"];
			if(strcmp(code, "Param") == 0) {
				doc["MtSpd"] = g_motor_speed;
				doc["MtSpd_LT"] = g_motor_speed_on_left_turn;
				doc["MtSpd_RT"] = g_motor_speed_on_right_turn;
				doc["MtLv_LT"] = g_lr_level_on_left_turn;
				doc["MtLv_RT"] = g_lr_level_on_right_turn;
				char payload[200];
				serializeJson(doc, payload);
				client.publish(mqttTopic_Param, payload);
			}
		}
	}
}

// MQTT Client ���ڑ��ł��Ȃ�������ڑ��ł���܂ōĐڑ������݂邽�߂� MQTT_reconnect �֐�
void MQTT_reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqttDeviceId, mqttUser, mqttPassword)) {
      Serial.println("connected");
      client.subscribe(mqttTopic_Signal);
      client.subscribe(mqttTopic_Command);
      client.subscribe(mqttTopic_Query);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5sec before retrying
      vTaskDelay(5000);
    }
  }
}
#endif

void LOG_output(const char str[], int level=99)
{
	if(g_log_level <= level) {
		Serial.println(str);
	}	
}

void SERVO_set_angle(int angle)
{
	if(angle < -90.0) {
		angle = -90.0;
	}
	if(angle > 90.0) {
		angle = 90.0;
	}
	angle -= 10;

	int val = (int)(servo_coeff_a*angle + servo_coeff_b);
	ledcWrite(DAC_CH_SERVO, val);
	vTaskDelay(500);
	ledcWrite(DAC_CH_SERVO, 0);	
}

float SR04_get_distance(unsigned long max_dist=100)   // return:[cm](timeout��������10000.0)  max_dist:[cm]
{
	// Pulse����
	digitalWrite(IO_PIN_US_TRIG, LOW);   
	delayMicroseconds(2);
	digitalWrite(IO_PIN_US_TRIG, HIGH);  
	delayMicroseconds(20);
	digitalWrite(IO_PIN_US_TRIG, LOW);   
	
	// ���˔g���B�܂ł̎��Ԍv��(timeout��������0)
	unsigned long duration = pulseIn(IO_PIN_US_ECHO, HIGH, max_dist*58);  
	float dist;
	if(duration == 0) {
		dist = 10000.0;
	}
	else {
		dist = duration * 0.017; // [cm]
	}

	return dist;
}  

void MOTOR_set_speed_left(int dir, int speed)
{
	ledcWrite(DAC_CH_MOTOR_A, speed);  
	g_motor_speed_right = speed;

	if(dir == MOTOR_DIR_FWD) {
		digitalWrite(IO_PIN_MOTOR_1,HIGH);
		digitalWrite(IO_PIN_MOTOR_2,LOW);
	}
	else if(dir == MOTOR_DIR_REV) {
		digitalWrite(IO_PIN_MOTOR_1,LOW);
		digitalWrite(IO_PIN_MOTOR_2,HIGH);
	}
}

void MOTOR_set_speed_right(int dir, int speed)
{
	ledcWrite(DAC_CH_MOTOR_B, speed);  
	g_motor_speed_left = speed;

	if(dir == MOTOR_DIR_FWD) {
		digitalWrite(IO_PIN_MOTOR_3,LOW);
		digitalWrite(IO_PIN_MOTOR_4,HIGH);
	}
	else if(dir == MOTOR_DIR_REV) {
		digitalWrite(IO_PIN_MOTOR_3,HIGH);
		digitalWrite(IO_PIN_MOTOR_4,LOW);
	}
}

int _us_get_distance()   // return:[cm](timeout��������10000)
{
	float dist; 
	float dist_sum = 0.0; 
	int i;
	for(i = 0; i < 5; i ++) {
		dist = SR04_get_distance(50); 
		if(dist > 9000.0) {
			// timeout����x�ł��v�����ꂽ�Ƃ��͑O���ɉ����Ȃ��ƌ��Ȃ�
			return	10000;
		}
		dist_sum += dist;
	}
	
	return	(int)(dist_sum*0.2); // 5�Ŋ�������0.2���|����
}

void _move_forward(int speed)
{
	MOTOR_set_speed_right(MOTOR_DIR_FWD, speed);
	MOTOR_set_speed_left(MOTOR_DIR_FWD, speed);
	
	g_state_motor = STATE_MOTOR_MOVING_FORWARD;
	
	LOG_output("go forward", 1);
}

void _move_backward(int speed)
{
	MOTOR_set_speed_right(MOTOR_DIR_REV, speed);
	MOTOR_set_speed_left(MOTOR_DIR_REV, speed);
	
	g_state_motor = STATE_MOTOR_MOVING_BACKWARD;
	
	LOG_output("go backward", 1);
}

void _turn_left(int dir, int speed, int level)
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

	g_state_motor = (dir==MOTOR_DIR_FWD) ? STATE_MOTOR_TURNING_LEFT : STATE_MOTOR_TURNING_LEFT_BACK;
	
	LOG_output("turn left!", 1);
}

void _turn_right(int dir, int speed, int level)
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
	
	g_state_motor = (dir==MOTOR_DIR_FWD) ? STATE_MOTOR_TURNING_RIGHT : STATE_MOTOR_TURNING_RIGHT_BACK;
	
	LOG_output("turn right!", 1);
}

void _rotate_ccw(int speed)
{
	MOTOR_set_speed_right(MOTOR_DIR_FWD, speed);
	MOTOR_set_speed_left(MOTOR_DIR_REV, speed);
	
	g_state_motor = STATE_MOTOR_ROTATING_CCW;
	
	LOG_output("rotate ccw!", 1);
}

void _rotate_cw(int speed)
{
	MOTOR_set_speed_right(MOTOR_DIR_REV, speed);
	MOTOR_set_speed_left(MOTOR_DIR_FWD, speed);
	
	g_state_motor = STATE_MOTOR_ROTATING_CW;
	
	LOG_output("rotate cw!", 1);
}

void _stop()
{
	MOTOR_set_speed_left(0, 0);
	MOTOR_set_speed_right(0, 0);

	g_state_motor = STATE_MOTOR_STOP;

	LOG_output("Stop!", 1);
}

float	g_temperature = 0.0;
float	g_max_diff_axl = 0.0;
unsigned short g_ps_val;
float g_als_val;
void _Task_sensor(void* param)
{
	int error;
	float	acc_x, acc_y, acc_z;
	float	gyro_x, gyro_y, gyro_z;
	BaseType_t xStatus;
	byte rc;
	float pre_abs_axl = 0;

	xSemaphoreGive(g_xMutex);
	for(;;) {
		vTaskDelay(50);

		// �����x�A�p���x�A���x���擾
		error = MPU6050_get_all(&acc_x, &acc_y, &acc_z, &gyro_x, &gyro_y, &gyro_z, &g_temperature);
		// �Ռ����o
		float abs_axl = (acc_x*acc_x + acc_y*acc_y + acc_z*acc_z);
		float diff_axl = abs_axl - pre_abs_axl;
		if(diff_axl < 0) {
			diff_axl = -diff_axl;
		}
		pre_abs_axl = abs_axl;

		// �Ɠx�E�ߐڃZ���T�̒l���擾
		unsigned short ps_val;
		float als_val;
		rc = rpr0521rs.get_psalsval(&ps_val, &als_val);
		if(rc == 0) {
		}

		if(diff_axl < 0.5) {
			digitalWrite(IO_PIN_LED,LOW);
		}
		else {
			digitalWrite(IO_PIN_LED,HIGH);
			if(diff_axl > 1.5) { // �Ռ����傫����������stop������
				int getstr = 's';
				xQueueSend(g_xQueue_Serial, &getstr, 100);
			}
		}

		if(als_val > 10.0) {
			digitalWrite(IO_PIN_LED2,LOW);
		}
		else {
			digitalWrite(IO_PIN_LED2,HIGH);
		}

		// ������ [�r��������]�J�n ������
		xStatus = xSemaphoreTake(g_xMutex, 0);
		if(diff_axl > g_max_diff_axl) {
			g_max_diff_axl = diff_axl;
		}
		g_ps_val = ps_val;
		g_als_val = als_val;
		xSemaphoreGive(g_xMutex);
		// ������ [�r��������]�J�n ������
	}

}

void _Task_WiFi(void* param)
{
	for(;;) {
		vTaskDelay(200);

		if (!client.connected()) {
			MQTT_reconnect();
		}
		client.loop();
	}

}

void _Task_disp(void* param)
{
	BaseType_t xStatus;
	float	temperature = 0.0;
	float	max_diff_axl = 0.0;
	unsigned short ps_val;
	float als_val;
	portTickType wakeupTime = xTaskGetTickCount();
	int		count = 0;

	xSemaphoreGive(g_xMutex);
	for(;;) {
		vTaskDelayUntil(&wakeupTime, 3000);
		
		// ������ [�r��������]�J�n ������
		xStatus = xSemaphoreTake(g_xMutex, 0);
		temperature = g_temperature;
		max_diff_axl = g_max_diff_axl;
		ps_val = g_ps_val;
		als_val = g_als_val;
		g_max_diff_axl = 0.0; // ���Z�b�g
		xSemaphoreGive(g_xMutex);
		// ������ [�r��������]�J�n ������
		
		g_proficiency_score += max_diff_axl;

		// �n���x�\��
		count ++;
		if(count > 10) {
			count = 0;
			Serial.print("Score = ");
			Serial.print(g_proficiency_score, 2);
			Serial.println();
		}

		//----- �Z���T�l��MTQQ broker��publis
		// JSON�t�H�[�}�b�g�쐬
		StaticJsonDocument<200> doc_in;
		doc_in["AxlDiff"] = max_diff_axl;
		doc_in["Temp"] = temperature;
		doc_in["Bright"] = als_val;
		doc_in["Prox"] = ps_val;
		doc_in["P-Score"] = g_proficiency_score;
		// payload�ɃZ�b�g���ꂽJSON�`�����b�Z�[�W��publish
		char payload[200];
		serializeJson(doc_in, payload);
		client.publish(mqttTopic_Sensor, payload);
	}
	
}

void _Task_robo_car(void* param)
{
	BaseType_t xStatus;

	for(;;) {
		int getstr = 0;
		xStatus = xQueueReceive(g_xQueue_Serial, &getstr, portMAX_DELAY);
		if(xStatus == pdPASS) {
			if(getstr == 'a') {
				g_ctrl_mode = CTRLMODE_AUTO_DRIVE;
				_stop();
			}
			else if(getstr == 'm') {
				g_ctrl_mode = CTRLMODE_MANUAL_DRIVE;
				_stop();
			}

			if(g_ctrl_mode == CTRLMODE_MANUAL_DRIVE) {
				if(getstr=='f') {
					_move_forward(g_motor_speed);
				}
				else if(getstr=='b') {
					_move_backward(g_motor_speed);
				}
				else if(getstr=='l') {
					_rotate_ccw(g_motor_speed);
				}
				else if(getstr=='r') {
					_rotate_cw(g_motor_speed);
				}
				else if(getstr=='L') {
					_turn_left(MOTOR_DIR_FWD, g_motor_speed_on_left_turn, g_lr_level_on_left_turn);
				}
				else if(getstr=='R') {
					_turn_right(MOTOR_DIR_FWD, g_motor_speed_on_right_turn, g_lr_level_on_right_turn); // ������
				}
				else if(getstr=='C') {
					_turn_left(MOTOR_DIR_REV, g_motor_speed_on_left_turn, g_lr_level_on_left_turn);
				}
				else if(getstr=='D') {
					_turn_right(MOTOR_DIR_REV, g_motor_speed_on_right_turn, g_lr_level_on_right_turn);
				}
				else if(getstr=='s') {
					_stop();		 
				}
			}
			else if(g_ctrl_mode == CTRLMODE_AUTO_DRIVE) {
				int right_distance = 0, left_distance = 0, middle_distance = 0;

				if(getstr=='s') {
					g_ctrl_mode = CTRLMODE_MANUAL_DRIVE;
					_stop();		 
				}		
				else {
					middle_distance = _us_get_distance();

					if(middle_distance <= g_stop_distance) {     
						_stop();
						vTaskDelay(500); 	  
						SERVO_set_angle(-80);  
						vTaskDelay(1000);      
						right_distance = _us_get_distance();

						vTaskDelay(500);
						SERVO_set_angle(0);              
						vTaskDelay(1000);                                                  
						SERVO_set_angle(80);              
						vTaskDelay(1000); 
						left_distance = _us_get_distance();

						vTaskDelay(500);
						SERVO_set_angle(0);              
						vTaskDelay(1000);
						if((right_distance<=g_stop_distance) && (left_distance<=g_stop_distance)) {
							_move_backward(g_motor_speed);
							vTaskDelay(180);
						}
						else if((right_distance>9000) && (left_distance>9000)) {
							_rotate_cw(180); // CW/CCW�̂ǂ���ł��悢
							vTaskDelay(500);
						}
						else if(right_distance>left_distance) {
							_rotate_cw(180);
							vTaskDelay(500);
						}
						else if(right_distance<left_distance) {
							_rotate_ccw(150);
							vTaskDelay(500);
						}
						else {
							_move_forward(g_motor_speed);
						}
					}  
					else {
						_move_forward(g_motor_speed);
					}
				}
			}
		}
		else {
			Serial.println("[Error] Queue");
			if(uxQueueMessagesWaiting(g_xQueue_Serial) != 0) {
				while(1) {
					Serial.println("rtos queue receive error, stopped");
					vTaskDelay(1000);
				}
			}
		}
	}
	
}

void _Task_Serial(void* param)
{
	BaseType_t xStatus;

	for(;;) {
		vTaskDelay(30);
		
		int getstr = -1;
		if (Serial.available() > 0) { // ��M�����f�[�^�����݂���
			// Serial Port����ꕶ���ǂݍ���
			getstr = Serial.read();
		}

		// ��Q�����o
		int dist = _us_get_distance();
	    if(dist <= g_stop_distance) {
			if((g_state_motor==STATE_MOTOR_MOVING_FORWARD) || 
				(g_state_motor==STATE_MOTOR_TURNING_RIGHT) ||
				(g_state_motor==STATE_MOTOR_TURNING_LEFT)) 
			{
				getstr = 's';
			}
		}

		if(getstr != -1) {
	        xStatus = xQueueSend(g_xQueue_Serial, &getstr, 100);
	        if(xStatus != pdPASS) {
				Serial.println(getstr);
			}
		}
	}

}

void setup()
{
	byte rc;

	g_ctrl_mode = CTRLMODE_MANUAL_DRIVE;

	// Serial Port(USB) �����ݒ�(115200bps����Bluetooth���������ʐM�ł��Ȃ�)
	Serial.begin(9600);
	// Bluetooth �����ݒ�
	//SerialBT.begin("ESP32");
	// I2C �����ݒ�
	Wire.begin(IO_PIN_SDA, IO_PIN_SCL);
	// WiFi�����ݒ�
	WiFi.begin(ssid, password);
	int cnt = 0;
	int is_success = true;
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print("Connecting to ");
		Serial.println(ssid);
		cnt ++;
		if(cnt > 30) {
			is_success = false;
			break;
		}
	}
	if(is_success) {
		Serial.println("Connected to the WiFi network.");
		// WiFi �A�N�Z�X�|�C���g����t�^���ꂽIP�A�h���X
		Serial.print("WiFi connected IP address: ");
		Serial.println(WiFi.localIP());
		// MQTT Server�̐ݒ�
		client.setServer(mqttServer, mqttPort);
		// topic��subscribe�����Ƃ��̃R�[���o�b�N�֐���o�^
		client.setCallback(callback_MQTT);
		// MQTT broker�Ƃ̐ڑ�
		MQTT_reconnect();
	}
	else {
		Serial.println("[Errol] Cannot connected to the WiFi network");
	}


	pinMode(IO_PIN_US_ECHO, INPUT);    
	pinMode(IO_PIN_US_TRIG, OUTPUT);  
	pinMode(IO_PIN_LED, OUTPUT);
	pinMode(IO_PIN_LED2, OUTPUT);
	pinMode(IO_PIN_MOTOR_1,OUTPUT);
	pinMode(IO_PIN_MOTOR_2,OUTPUT);
	pinMode(IO_PIN_MOTOR_3,OUTPUT);
	pinMode(IO_PIN_MOTOR_4,OUTPUT);
	pinMode(IO_PIN_MOTOR_ENA,OUTPUT);
	pinMode(IO_PIN_MOTOR_ENB,OUTPUT);
	pinMode(IO_PIN_SERVO,OUTPUT);
	pinMode(IO_PIN_LINETRACK_LEFT, INPUT);    
	pinMode(IO_PIN_LINETRACK_CENTER, INPUT);    
	pinMode(IO_PIN_LINETRACK_RIGHT, INPUT);    

	// Motor�̏����ݒ�
	ledcSetup(DAC_CH_MOTOR_A, 980, 8);
	ledcSetup(DAC_CH_MOTOR_B, 980, 8);
	ledcAttachPin(IO_PIN_MOTOR_ENA, DAC_CH_MOTOR_A);
	ledcAttachPin(IO_PIN_MOTOR_ENB, DAC_CH_MOTOR_B);

	_stop();

	// Servo�̏����ݒ�
	float servo_min = 26.0;  // (26/1024)*20ms �� 0.5 ms  (-90��)
	float servo_max = 123.0; // (123/1024)*20ms �� 2.4 ms (+90��)
	servo_coeff_a = (servo_max-servo_min)/180.0;
	servo_coeff_b = (servo_max+servo_min)/2.0;
	ledcSetup(DAC_CH_SERVO, 50, 10);  // 0ch 50 Hz 10bit resolution
	ledcAttachPin(IO_PIN_SERVO, DAC_CH_SERVO); 

    SERVO_set_angle(0);//********xxxxx setservo position according to scaled value
    delay(500); 

  	// �Ɠx�E�ߐڃZ���T�̏����ݒ�
	rc = rpr0521rs.init();
	if(rc != 0) {
		Serial.println("[Error] cannot initialize RPR-0521.");
	}

	// �����x�Z���T������
	MPU6050_init(&Wire);

	// LED�_��
	for(int i = 0; i < 3; i ++) {
		digitalWrite(IO_PIN_LED, HIGH);
		delay(1000);
		digitalWrite(IO_PIN_LED, LOW);
		delay(1000);
	}

	// �R�A0�Ŋ֐�task0��stack�T�C�Y4096,�D�揇��1(�傫���قǗD��x��)�ŋN��
	g_xQueue_Serial = xQueueCreate(8, sizeof(int32_t));

	g_xMutex = xSemaphoreCreateMutex();
	xTaskCreatePinnedToCore(_Task_sensor, "Task_sensor", 2048, NULL, 5, NULL, 0);
	xTaskCreatePinnedToCore(_Task_WiFi, "Task_WiFi", 2048, NULL, 2, NULL, 0);
	xTaskCreatePinnedToCore(_Task_disp, "Task_disp", 2048, NULL, 1, NULL, 0);
	xTaskCreatePinnedToCore(_Task_robo_car, "Task_robo_car", 2048, NULL, 3, NULL, 0);
	xTaskCreatePinnedToCore(_Task_Serial, "Task_Serial", 2048, NULL, 4, NULL, 0);

	// payload�ɃZ�b�g���ꂽJSON�`�����b�Z�[�W�𓊍e
	char text[200];
	sprintf(text, "{\"Init\":\"Pass\"}");
	client.publish(mqttTopic_Status, text);

	Serial.println("Completed setup program successfully.");
}

void loop()
{


}

