////////////////////////////////////////////////////////////////////////////
// Smart Car
////////////////////////////////////////////////////////////////////////////

#include "esp_system.h"

#define	PIN_SERVO		(25)
#define PIN_US_ECHO		(36)
#define PIN_US_TRIG		(5)

float servo_coeff_a;
float servo_coeff_b;

void _servo_angle(float angle)
{
	if(angle < -90.0) {
		angle = -90.0;
	}
	if(angle > 90.0) {
		angle = 90.0;
	}

	int val = (int)(servo_coeff_a*angle + servo_coeff_b);
	ledcWrite(0, val);
}

int _us_get_distance()   
{
	digitalWrite(PIN_US_TRIG, LOW);   
	delayMicroseconds(20);
	digitalWrite(PIN_US_TRIG, HIGH);  
	delayMicroseconds(20);
	digitalWrite(PIN_US_TRIG, LOW);   
	float Fdistance = pulseIn(PIN_US_ECHO, HIGH);  
	Fdistance= Fdistance/58;       

	return (int)Fdistance;
}  

void setup()
{ 
	float servo_min = 26.0;  // (26/1024)*20ms �� 0.5 ms  (-90��)
	float servo_max = 123.0; // (123/1024)*20ms �� 2.4 ms (+90��)
	servo_coeff_a = (servo_max-servo_min)/180.0;
	servo_coeff_b = (servo_max+servo_min)/2.0;
	ledcSetup(0, 50, 10);  // 0ch 50 Hz 10bit resolution
	ledcAttachPin(PIN_SERVO, 0); // 15pin, 0ch

	pinMode(PIN_US_ECHO, INPUT);    
	pinMode(PIN_US_TRIG, OUTPUT);  
	Serial.begin(115200);


    _servo_angle(90);
    delay(500); 
}

void loop()
{ 

	_servo_angle(-85.0);
	delay(2000);
	_servo_angle(0.0);
	delay(2000);
	_servo_angle(85.0);
	delay(2000);
/*
	int dist = _us_get_distance();

	Serial.println(dist);
*/

	delay(2000);
}

/*
int n = min;
void loop() {
  ledcWrite(0, n);
  n+=5;
  if (n > max) n = min;
  delay(500);
}
*/

/*
久保
* /
 */
