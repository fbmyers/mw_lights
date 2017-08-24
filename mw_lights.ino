#include "mw_lights.h"
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
char rainbow_saturation = 180;
char rainbow_value = 20;

//state variables
static float last_transition_t = 0.0;
uint32_t display_color;
uint32_t ring_color[12];
bool is_paused;
double t;
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
	static uint8_t current_positions[4] = { 0,0,0,0 };
	static uint8_t last_positions[4] = { 0,0,0,0 };
	static HsvColor current_rainbow_color;
	static bool blink_on;

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
			last_positions[0] = current_positions[0];

			//move on to next segment
			if (ring_chase_direction)
				current_positions[0] = current_positions[0]>=11 ? 0 : current_positions[0] + 1;
			else
				current_positions[0] = current_positions[0]<=0 ? 11 : current_positions[0] - 1;
		}
		crossfade(ring_fg_color, ring_bg_color, &ring_color[last_positions[0]], &ring_color[current_positions[0]]);

		break;
    case MODE_BLINK:
		//random blinks of N panels at a time
		if ((t - last_transition_t) > ring_effect_time) {
			last_transition_t = t;

			if (!blink_on) //pick some new segments to turn on
				for (int i = 0; i < 4; i++)
					current_positions[i] = random(0, 11);
		
			blink_on = !blink_on;
		}

		uint32_t dummy;
		if (blink_on) 
			for (int i = 0; i < ring_blink_segments; i++) //fade in
				crossfade(ring_fg_color, ring_bg_color, &dummy, &ring_color[current_positions[i]]);
		else
			for (int i = 0; i < ring_blink_segments; i++) //fade out
				crossfade(ring_bg_color, ring_fg_color, &dummy, &ring_color[current_positions[i]]);

		break;
    case MODE_RAINBOW:
		//cycle through a color palette
		if ((t - last_transition_t) > ring_effect_time) {
			last_transition_t = t;
			//take the current fg and bg colors and shift them by +10 hue in HSV space
			//i'm being very lazy with this...
			current_rainbow_color.h++;
			current_rainbow_color.s = rainbow_saturation;
			current_rainbow_color.v = rainbow_value;
			RgbColor rgb = HsvToRgb(current_rainbow_color);
			ring_bg_color = Adafruit_NeoPixel::Color(rgb.r, rgb.g, rgb.b, 0);
		}
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
		rainbow_saturation = atoi(params[4]);
		rainbow_value = atoi(params[5]);
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

//i just grabbed these off the internet. could be cleaned up and harmonized w/ existing color representations
RgbColor HsvToRgb(HsvColor hsv)
{
	RgbColor rgb;
	unsigned char region, remainder, p, q, t;

	if (hsv.s == 0)
	{
		rgb.r = hsv.v;
		rgb.g = hsv.v;
		rgb.b = hsv.v;
		return rgb;
	}

	region = hsv.h / 43;
	remainder = (hsv.h - (region * 43)) * 6;

	p = (hsv.v * (255 - hsv.s)) >> 8;
	q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
	t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

	switch (region)
	{
	case 0:
		rgb.r = hsv.v; rgb.g = t; rgb.b = p;
		break;
	case 1:
		rgb.r = q; rgb.g = hsv.v; rgb.b = p;
		break;
	case 2:
		rgb.r = p; rgb.g = hsv.v; rgb.b = t;
		break;
	case 3:
		rgb.r = p; rgb.g = q; rgb.b = hsv.v;
		break;
	case 4:
		rgb.r = t; rgb.g = p; rgb.b = hsv.v;
		break;
	default:
		rgb.r = hsv.v; rgb.g = p; rgb.b = q;
		break;
	}

	return rgb;
}

HsvColor RgbToHsv(uint8_t r, uint8_t g, uint8_t b)
{
	HsvColor hsv;
	RgbColor rgb;
	rgb.r = r;
	rgb.g = g;
	rgb.b = b;

	unsigned char rgbMin, rgbMax;

	rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
	rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

	hsv.v = rgbMax;
	if (hsv.v == 0)
	{
		hsv.h = 0;
		hsv.s = 0;
		return hsv;
	}

	hsv.s = 255 * long(rgbMax - rgbMin) / hsv.v;
	if (hsv.s == 0)
	{
		hsv.h = 0;
		return hsv;
	}

	if (rgbMax == rgb.r)
		hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
	else if (rgbMax == rgb.g)
		hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
	else
		hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

	return hsv;
}