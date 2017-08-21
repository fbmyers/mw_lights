#include <Adafruit_NeoPixel.h>

//SERIAL I/O
#define CMD_DELIM '\n'
#define MAX_CMD_LENGTH 200
char uart_buffer[MAX_CMD_LENGTH + 1];
uint16_t buffer_pos = 0;

#define PIXEL_COUNT 30

#define LED_PIN 3

#define MODE_SOLID 0
#define MODE_CHASE 1
#define MODE_BLINK 2
#define MODE_RAINBOW 3
#define MODE_MANUAL 4

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXEL_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

const char display_pixels[] = {0,4};
const char ring_pixels[12][2] = {{5,6},
                                 {7,8},
                                 {9,10},
                                 {11,12},
                                 {13,14},
                                 {15,16},
                                 {17,18},
                                 {19,20},
                                 {21,22},
                                 {23,24},
                                 {25,26},
                                 {27,28}};
                                                                  

float display_freq = 0.5*(2*PI);
float display_amp = 30.0;
float display_offset = 60.0;
uint32_t ring_bg_color = Adafruit_NeoPixel::Color(0, 10, 40, 0);
uint32_t ring_fg_color = Adafruit_NeoPixel::Color(0, 90, 70, 0);
float ring_transition_time = 0.5;
float ring_effect_time = 3.0;
int ring_blink_segments = 3;
bool ring_chase_direction = true;

uint32_t display_color;
uint32_t ring_color[12];

float t;

unsigned char mode = MODE_CHASE;

void setup()
{
	Serial.begin(115200);
	buffer_pos = 0;
	uart_buffer[buffer_pos] = '\0';

	pixels.begin();
}

void loop()
{
	t = millis()/1000.0;
	serialPoll();
	displayPoll();
	ringPoll();
 
	pixels.show();

}

void displayPoll() {
  uint8_t w = display_amp*sin(t*display_freq) + display_offset;

  if (mode != MODE_MANUAL)
	  display_color = Adafruit_NeoPixel::Color(0, 0, 0, w);

  for (int i = display_pixels[0]; i<=display_pixels[1]; i++) {
    pixels.setPixelColor(i, display_color);
  }
}

void ringPoll() {
  static float last_transition_t = 0.0;
  static char current_position = 0;

  //initialize everything to the background color, unless this is in manual mode
  if (mode!=MODE_MANUAL)
	for (int i = 0; i < 12; i++)
	  ring_color[i] = ring_bg_color;

  switch (mode) { 
    case MODE_SOLID:
		break;
    case MODE_CHASE:
		if ((t - last_transition_t) > ring_effect_time) {
			//move on to next segment
			if (ring_chase_direction)
				current_position = current_position>11 ? 0 : current_position + 1;
			else
				current_position = current_position<0 ? 11 : current_position - 1;
			last_transition_t = t;
		}
		ring_color[current_position] = ring_fg_color;
		break;
    case MODE_BLINK:

		break;
    case MODE_RAINBOW:

		break;
  }

  //write LED values
  for (int i = 0; i < 12; i++)
	  for (int j = ring_pixels[i][0]; j <= ring_pixels[i][1]; j++) {
		  pixels.setPixelColor(j, ring_color[i]);
	  }
}



void serialPoll()
{
	//TODO: move to interrupt
	char new_char = 0;
	while (Serial.available() > 0) {
		
		new_char = (char)Serial.read();
		if (new_char == CMD_DELIM) {
			//a complete command has been received, so act on it
			parseCommand();
			buffer_pos = 0;
			uart_buffer[buffer_pos] = '\0';
		}
		else {
			uart_buffer[buffer_pos++] = new_char;
			uart_buffer[buffer_pos] = '\0';
		}
	}
}

void parseCommand() {
	Serial.println(uart_buffer);

	Serial.println("parsing");
	char* cmd = strtok(uart_buffer, " ");
	char* params[10];
	for (int i = 0; i < 10; i++)
		params[i] = strtok(NULL, " ");
	
	Serial.println(cmd);
	Serial.println(params[0]);
	Serial.println(params[1]);
	Serial.println(params[2]);

	if (strcmp(cmd, "leddispparams") == 0)
	{
		display_amp = atof(params[0]);
		display_freq = atof(params[1]);
		display_offset = atof(params[2]);

	}
	else if (strcmp(cmd, "ledmode") == 0)
	{
		mode = atoi(params[0]);
	}
	else if (strcmp(cmd, "ledringfg") == 0)
	{

	}
	else if (strcmp(cmd, "ledringbg") == 0)
	{

	}
	if (strcmp(cmd, "ledringparams") == 0)
	{
		ring_effect_time = atof(params[0]);
		ring_transition_time = atof(params[1]);
		ring_blink_segments = atoi(params[2]);
		ring_chase_direction = atoi(params[3]);

	}
}
