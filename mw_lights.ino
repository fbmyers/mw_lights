#include <Adafruit_NeoPixel.h>

//SERIAL I/O
#define CMD_DELIM '\n'
#define CMD_PAUSE_PIXELS '#'
#define MAX_CMD_LENGTH 200
char uart_buffer[MAX_CMD_LENGTH + 1];
uint16_t buffer_pos = 0;

#define PIXEL_COUNT 150

#define LED_PIN 3

#define MODE_SOLID 0
#define MODE_CHASE 1
#define MODE_BLINK 2
#define MODE_RAINBOW 3
#define MODE_MANUAL 4

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXEL_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

const uint16_t display_pixels[] = {0,4};
const uint16_t ring_pixels[12][2] = {{5,6},
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
                                 {27,149}};
                                                                  

//settings
float display_freq = 0.3;
float display_amp = 20.0;
float display_offset = 80.0;
uint32_t ring_bg_color = Adafruit_NeoPixel::Color(0, 10, 40, 0);
uint32_t ring_fg_color = Adafruit_NeoPixel::Color(0, 70, 80, 20);
float crossfade_time = 0.4;
float ring_effect_time = 0.5;
int ring_blink_segments = 3;
bool ring_chase_direction = true;

//state variables
static float last_transition_t = 0.0;
uint32_t display_color;
uint32_t ring_color[12];
bool is_paused;
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
	//to make pause/play appear seamless, we only increment t if not paused.
	static uint32_t last_millis = 0;
	uint32_t current_millis = millis();

	if (!is_paused)
		t += (current_millis -last_millis)/1000.0;
	last_millis = current_millis;

	serialPoll();
	displayPoll();
	ringPoll();
 
	//the pause command ('#') must be sent before a string command can be processed
	if (!is_paused)
		pixels.show();

}

void displayPoll() {
  
  if (mode != MODE_MANUAL) {
	  //pulse white light as a sin()
	  uint8_t w = display_amp*sin(t*display_freq*2*PI) + display_offset;
	  display_color = Adafruit_NeoPixel::Color(0, 0, 0, w);
  }

  for (int i = display_pixels[0]; i<=display_pixels[1]; i++) {
    pixels.setPixelColor(i, display_color);
  }
}

void ringPoll() {
  static uint8_t current_position = 0;
  static uint8_t last_position = 0;

  //initialize everything to the background color, unless this is in manual mode
  if (mode!=MODE_MANUAL)
	for (int i = 0; i < 12; i++)
	  ring_color[i] = ring_bg_color;

  switch (mode) { 
    case MODE_SOLID:
		//all ring LEDs remain @ background color

		break;
    case MODE_CHASE:
		//foreground color chases around ring

		if ((t - last_transition_t) > ring_effect_time) {
			last_transition_t = t;
			last_position = current_position;

			//move on to next segment
			if (ring_chase_direction)
				current_position = current_position>=11 ? 0 : current_position + 1;
			else
				current_position = current_position<=0 ? 11 : current_position - 1;
		}
		crossfade(ring_fg_color, ring_bg_color, &ring_color[last_position], &ring_color[current_position]);

		break;
    case MODE_BLINK:
		//random blinks of N panels at a time

		break;
    case MODE_RAINBOW:
		//cycle through a color palette

		break;
  }

  //write LED values
  for (int i = 0; i < 12; i++)
	  for (int j = ring_pixels[i][0]; j <= ring_pixels[i][1]; j++) {
		  pixels.setPixelColor(j, ring_color[i]);
	  }
}

void crossfade(uint32_t to_color, uint32_t from_color, uint32_t* backward, uint32_t* foreward)
{
	float t_c = (t - last_transition_t);

	if (t_c < crossfade_time) {
		uint8_t r0, g0, b0, w0;
		uint8_t r1, g1, b1, w1;
		uint8_t rf, gf, bf, wf;
		uint8_t rb, gb, bb, wb;

		colorToRGBW(from_color, &r0, &g0, &b0, &w0);
		colorToRGBW(to_color, &r1, &g1, &b1, &w1);

		float progress = t_c / crossfade_time;
		rf = r0 + (r1 - r0)*progress;
		gf = g0 + (g1 - g0)*progress;
		bf = b0 + (b1 - b0)*progress;
		wf = w0 + (w1 - w0)*progress;

		rb = r1 + (r0 - r1)*progress;
		gb = g1 + (g0 - g1)*progress;
		bb = b1 + (b0 - b1)*progress;
		wb = w1 + (w0 - w1)*progress;

		*foreward = Adafruit_NeoPixel::Color(rf,gf,bf,wf);
		*backward = Adafruit_NeoPixel::Color(rb, gb, bb, wb);
	}
	else {
		*foreward = to_color;
		*backward = from_color;
	}


}

void colorToRGBW(uint32_t c, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* w)
{
	*w = 0xFF & (c >> 24);
	*r = 0xFF & (c >> 16);
	*g = 0xFF & (c >> 8);
	*b = 0xFF & (c >> 0);

}

void serialPoll()
{
	//TODO: move to interrupt
	char new_char = 0;
	while (Serial.available() > 0) {
		new_char = (char)Serial.read();
		if (new_char == CMD_PAUSE_PIXELS) {
			is_paused = !is_paused;
		}
		else if (new_char == CMD_DELIM) {
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
	char* prefix = strtok(uart_buffer, " ");
	char* cmd = strtok(NULL, " ");
	char* params[10];
	for (int i = 0; i < 10; i++)
		params[i] = strtok(NULL, " ");

	//all commands coming form mamaduino should start with "led"
	if (strcmp(prefix, "led") != 0)
	{
		Serial.println("Invalid Command");
		return;
	}

	if (strcmp(cmd, "dispparams") == 0)
	{
		display_amp = atof(params[0]);
		display_freq = atof(params[1]);
		display_offset = atof(params[2]);

	}
	else if (strcmp(cmd, "mode") == 0)
	{
		mode = atoi(params[0]);
	}
	else if (strcmp(cmd, "ringfg") == 0)
	{
		ring_fg_color = Adafruit_NeoPixel::Color(atoi(params[0]),
												 atoi(params[1]),
												 atoi(params[2]),
												 atoi(params[3]));
	}
	else if (strcmp(cmd, "ringbg") == 0)
	{
		ring_bg_color = Adafruit_NeoPixel::Color(atoi(params[0]),
												 atoi(params[1]),
												 atoi(params[2]),
												 atoi(params[3]));
	}
	else if (strcmp(cmd, "ringparams") == 0)
	{
		ring_effect_time = atof(params[0]);
		crossfade_time = atof(params[1]);
		ring_blink_segments = atoi(params[2]);
		ring_chase_direction = atoi(params[3]);
	}
	else if (strcmp(cmd, "set") == 0)
	{
		int idx = atoi(params[0]);
		if (idx == 0)
			display_color = Adafruit_NeoPixel::Color(atoi(params[1]),
													 atoi(params[2]),
													 atoi(params[3]),
													 atoi(params[4]));
		else
			ring_color[idx-1] = Adafruit_NeoPixel::Color(atoi(params[1]),
				atoi(params[2]),
				atoi(params[3]),
				atoi(params[4]));

	}
}
