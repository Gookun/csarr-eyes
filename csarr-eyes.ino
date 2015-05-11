#include "Adafruit_GFX.h"
#include <SPI.h>
#include "Max72xxPanel.h"
#include "robot_eye_i2c.h"
#include <Wire.h>



int pinCS = 10; // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
int numberOfHorizontalDisplays = 6;
int numberOfVerticalDisplays = 1;

Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);


int wait = 50; // In milliseconds
int second_in_ms = 1000;
int spacer = 1;
int width = 5 + spacer; // The font width is 5 pixels

enum MatrixMode { EYE, TEXT, HEART, COUNTDOWN, CONFUSE  };
enum EyeMode {NORMAL, LOOKING_LEFT, LOOKING_RIGHT, LOOKING_UP, LOOKING_DOWN, CROSS_EYED, RANDOM};
String CSARR = "Vive CSARR !";
String RDY = "GEORGE EST PRET";
String MAMMOUTH = "VISE LE MAMMOUTH";

MatrixMode currentMode = EYE;
EyeMode currentEyeMode = RANDOM;
String currentText = RDY;

int countdown_index = 9;
int confuse_index = 0;
char countdown[10] = {'0','1','2','3','4','5','6','7','8','9'};
uint32_t last_image_timestamp = 0;

//Eyes shifting
int shift_first_eye = 8;
int shift_second_eye = 32;

// Bitmaps
static const uint8_t PROGMEM blink_1[8] = {B00000000, B00111100, B01111110, B01111110, B01111110, B01111110, B00111100, B00000000};
static const uint8_t PROGMEM blink_2[8] = {B00000000, B00000000, B00111100, B01111110, B01111110, B00111100, B00000000, B00000000};
static const uint8_t PROGMEM blink_3[8] = {B00000000, B00000000, B00000000, B01111110, B01111110, B00000000, B00000000, B00000000};
static const uint8_t PROGMEM blink_4[8] = {B00000000, B00000000, B00000000, B00000000, B01111110, B00000000, B00000000, B00000000};
static const uint8_t PROGMEM blink_5[8] = {B00000000, B00000000, B00000000, B00000000, B00000000, B00000000, B00000000, B00000000};

static const uint8_t PROGMEM heart_1[8] = {B00110110, B01111111, B01111111, B01111111, B00111110, B00011100, B00001000, B00000000};
static const uint8_t PROGMEM heart_2[8] = {B00000000, B01101100, B11111110, B11111110, B11111110, B01111100, B00111000, B00010000};

//static const uint8_t PROGMEM confuse[8] = {B00000001, B01111101, B01000101, B01010101, B01011101, B01000001, B01111111, B00000000};
static const uint8_t PROGMEM confuse[8] = {B00000001, B00111001, B01000101, B01010101, B01011101, B01000001, B00111110, B00000000};

// Animations
static const uint8_t *blinkImg[]={blink_1, blink_2, blink_3, blink_4, blink_5};
static const uint8_t *heart[]={heart_1, heart_2};

uint8_t 
	blinkIndex[] = { 0, 1, 2, 3, 4, 3, 2, 1, 0},
	heartIndex = 0,
	heartCounter = 10,
	blinkCountdown = 10,
	gazeCountdown  =  20, // Countdown to next eye movement
	gazeFrames     =  30; // Duration of eye movement (smaller = faster)

int8_t
	eyeX = 3, eyeY = 3,   // Current eye position
	newX = 3, newY = 3,   // Next eye position
	dX   = 0, dY   = 0;   // Distance from prior to new position

static const uint8_t PROGMEM img[] = {B00000000, B00000000, B00000000, B01111110, B01111110, B00000000, B00000000, B00000000};

void setup() {
	randomSeed(analogRead(A0));
	Wire.begin(I2C_ROBOT_EYE_ADDRESS);
	Wire.onReceive(receiveI2CEvent);
	Wire.onRequest(sendResponse);
	matrix.setIntensity(1); // Use a value between 0 and 15 for brightness
    Serial.begin(115200);
}

void loop() {

	switch (currentMode)
	{
	case EYE:
		eye_mode_perform();
		break;
	case TEXT:
		text_mode_perform();
		break;
	case HEART:
		heart_mode_perform();
		break;
	case COUNTDOWN:
		countdown_mode_perform();
		break;
	case CONFUSE:
		confuse_mode_perform();
		break;
	default:
		break;
	}
        

}

void receiveI2CEvent(int howMany)
{
        Serial.print("event :");
	int x = Wire.read();    // receive byte as an integer
        Serial.println(x);
	switch(x){
	// ALL PASSTHROUGH (as only random as defined xD)
	case I2C_LOOK_MIDDLE:
	case I2C_LOOK_RIGHT:
	case I2C_LOOK_LEFT:
	case I2C_LOOK_UP:
	case I2C_LOOK_DOWN:
	case I2C_CROSS_EYED:
	case I2C_RANDOM:
		currentMode = EYE;
		currentEyeMode = RANDOM;
		break;
	case I2C_MODE_COUNTDOWN:
		currentMode = COUNTDOWN;
		break;
	case I2C_TEXT_CSARR:
		currentMode = TEXT;
		currentText = CSARR;
		break;
	case I2C_TEXT_RDY:
		currentMode = TEXT;
		currentText = RDY;
		break;
	case I2C_MODE_HEART:
		currentMode = HEART;
		break;
	case I2C_CONFUSE:
		currentMode = CONFUSE;
		break;
	}

}

void sendResponse(){
	//only for debug.
	Wire.write(currentMode);
}

void eye_mode_perform(){
	if(millis() >= last_image_timestamp + wait){
		matrix.setRotation(2);
		matrix.fillScreen(LOW);
		matrix.drawBitmap(0 + shift_first_eye, 0,
			blinkImg[
				(blinkCountdown < sizeof(blinkIndex)) ? // Currently blinking?
					blinkIndex[blinkCountdown] : 0          // No, show bitmap 0
			], 8, 8, HIGH);
		matrix.drawBitmap(0 + shift_second_eye, 0,blinkImg[
				(blinkCountdown < sizeof(blinkIndex)) ? // Currently blinking?
					blinkIndex[blinkCountdown] : 0          // No, show bitmap 0
			], 8, 8, HIGH);
		// Decrement blink counter.  At end, set random time for next blink.
		if(--blinkCountdown == 0) blinkCountdown = random(5, 100);

		if(--gazeCountdown <= gazeFrames) { //--gazeCountdown <= gazeFrames
			// Eyes are in motion - draw pupil at interim position
			matrix.fillRect(
				newX - (dX * gazeCountdown / gazeFrames) + shift_first_eye,
				newY - (dY * gazeCountdown / gazeFrames),
				2, 2, LOW);
			matrix.fillRect(
				newX - (dX * gazeCountdown / gazeFrames) + shift_second_eye,
				newY - (dY * gazeCountdown / gazeFrames),
				2, 2, LOW);
			if(gazeCountdown == 0) {    // Last frame?
				eyeX = newX; eyeY = newY; // Yes.  What's new is old, then...
				do { // Pick random positions until one is within the eye circle
					newX = random(3) + 2; newY = random(3) + 2;
					dX   = newX - 4;  dY   = newY - 4;
				} while((dX * dX + dY * dY) >= 16);      // Thank you Pythagoras
				dX            = newX - eyeX;             // Horizontal distance to move
				dY            = newY - eyeY;             // Vertical distance to move
				gazeCountdown = random(gazeFrames, 60); // Count to end of next movement
			}
		} else {
			// Not in motion yet -- draw pupil at current static position
			matrix.fillRect(eyeX +shift_first_eye, eyeY, 2, 2, LOW);
                        matrix.fillRect(eyeX +shift_second_eye, eyeY, 2, 2, LOW);
		}
		matrix.write();
		last_image_timestamp = millis();
	}
}

void text_mode_perform(){
        SPI.end();
        SPI.begin();
	for ( int i = 0 ; i < width * currentText.length() + matrix.width() - 1 - spacer; i++ ) {
		matrix.setRotation(2); //monté à l'envert
		matrix.fillScreen(LOW);

		int letter = i / width;
		int x = (matrix.width() - 1) - i % width;
		int y = (matrix.height() - 8) / 2; // center the text vertically

		while ( x + width - spacer >= 0 && letter >= 0 ) {
			if ( letter < currentText.length() ) {
				matrix.drawChar(x, y, currentText[letter], HIGH, LOW, 1);
			}

			letter--;
			x -= width;
		}
		matrix.write(); // Send bitmap to display

		delay(wait-10);	
	}
        
        
}

void heart_mode_perform(){
	if(millis() >= (last_image_timestamp + wait)){
		if (--heartCounter == 0){
			heartCounter = 8;
			heartIndex = heartIndex ? 0 : 1;
		}
		matrix.setRotation(2);
		matrix.fillScreen(LOW);
		matrix.drawBitmap(0 + shift_first_eye,0, heart[heartIndex], 8, 8, HIGH);
        matrix.drawBitmap(0 + shift_second_eye,0, heart[heartIndex], 8, 8, HIGH);
		matrix.write();
		last_image_timestamp = millis();
	}
}

void countdown_mode_perform(){
	if(millis() >= (last_image_timestamp + second_in_ms)){
		if(countdown_index >= 0){
			matrix.setRotation(2);
			matrix.fillScreen(LOW);
			matrix.drawChar(1 + shift_first_eye, 1, countdown[countdown_index], HIGH,LOW,1);
			matrix.drawChar(1 + shift_second_eye, 1, countdown[countdown_index], HIGH,LOW,1);
			countdown_index--;
			matrix.write();
			last_image_timestamp = millis();
		}
		else{
			reset_countdown();
			currentMode = TEXT;
			currentText = CSARR;

		}
	}
}

void confuse_mode_perform(){
	if(millis() >= (last_image_timestamp + 2*wait)){
		confuse_index++;
		confuse_index%=3;
		matrix.fillScreen(LOW);
		matrix.drawBitmap(0,0, confuse, 8, 8, HIGH);
		matrix.setRotation(confuse_index);
		matrix.write();
		last_image_timestamp = millis();
	}
}

void reset_countdown(){
	countdown_index = 9;
}
