#include <frequencyToNote.h>
#include <LiquidCrystal.h>
#include "MIDIUSB.h"  // arduino DUEs can do this, I think Leonardos can, too
#include "PitchToNote.h"

/* @TODO: shift register */
#define NOTE_UP_PIN   6
#define NOTE_DOWN_PIN 5
#define PLAY_PIN      4
#define MARK_PIN      3
#define RUB_PIN       2
#define SUSTAIN_APIN  0

/* LCD, dangerzone: you can't use Serial if you use pin 1 or pin 0 for GPIO */
#define MY_LCD_RS 8
#define MY_LCD_E 9
#define MY_LCD_D7 10
#define MY_LCD_D6 11
#define MY_LCD_D5 12
#define MY_LCD_D4 13

#define REFRESH_DELAY 30

// LiquidCrystal lcd(13, 12, 1, 2, 8, 11);

LiquidCrystal lcd(MY_LCD_RS, MY_LCD_E,
	MY_LCD_D4, MY_LCD_D5, MY_LCD_D6, MY_LCD_D7);

#define DEBOUNCE_DELAY 150

unsigned int sustain = 50;
unsigned int last_sustain = 1;

bool dirty_editor = true;
bool dirty_player = true;

int analog_in = 0;

int scale[] = { 0, pitchD3, pitchE3, pitchF3, pitchG3, pitchA3, pitchB3b, pitchC4, pitchD4 };
int scale_max_size = 9;

/* int sequence[] = {
	pitchD3, pitchD4, pitchD5, pitchC3,
	pitchC4, pitchC5, pitchC4, pitchD4
}; */

int last_note = 0;


int sequence[] = {
	5, 2, 3, 4,
	1, 0, 3, 6,
	0, 1, 2, 4,
	3, 1, 3, 6
};

/*
int sequence[] = {
	0, 0, 0, 0,
	0, 0, 0, 6,
	0, 1, 2, 4,
	3, 1, 3, 6
}; */

int last_sequence[] = {
	1, 1, 3, 1,
	1, 1, 3, 6
};

int sequence_position = 0;
int sequence_max = 8;
int sequencer_pointer = 0;  // misnomer: not a pointer like you're thinking
int last_sequencer_pointer = 0;  // misnomer: not a pointer like you're thinking
int last_lcd_update = 0;

int max_sustain = 1000;

#define PLAYMODE_MIDI 1
#define PLAYMODE_DAC 2
#define PLAYMODE_DAC_PIN DAC0
int playmode = PLAYMODE_MIDI;

bool playing = false;

unsigned long last_button_press_in_millis = 0;
unsigned long last_tone_play_in_millis = 0;

void verifyCheatcodes() {
	lcd.setCursor(0, 1);
	if (LOW == digitalRead(MARK_PIN)) {
		sequence_max = 16;
		lcd.print("BIG!");
		delay(500);
	}

	if (LOW == digitalRead(RUB_PIN)) {
		lcd.print("SHH!");
		// midi all off
		midiEventPacket_t noteOn = { 0b10110000, 0b10110000 | 0, 123 };
		delay(500);
	}

	if (LOW == digitalRead(NOTE_UP_PIN)) {
		lcd.print("LONG");
		max_sustain = 4000;
		delay(500);
	}

	if (LOW == digitalRead(NOTE_DOWN_PIN)) {
		playmode = PLAYMODE_DAC;
		lcd.print("DAC0");
		delay(500);
	}
}

void setup() {
	lcd.begin(16, 2);
	lcd.print("Good morning!");

	pinMode(NOTE_DOWN_PIN, INPUT_PULLUP);
	pinMode(NOTE_UP_PIN, INPUT_PULLUP);
	pinMode(PLAY_PIN, INPUT_PULLUP);
	pinMode(MARK_PIN, INPUT_PULLUP);
	pinMode(RUB_PIN, INPUT_PULLUP);

	pinMode(LED_BUILTIN, OUTPUT);

	last_button_press_in_millis = millis();
	last_tone_play_in_millis = millis();
  
	/* cheatcodes, bruh */
	verifyCheatcodes();
	lcd.clear();
}

// https://github.com/arduino-libraries/MIDIUSB/blob/master/examples/MIDIUSB_write/MIDIUSB_write.ino#L11
// First parameter is the event type (0x09 = note on, 0x08 = note off).
// Second parameter is note-on/note-off, combined with the channel.
// Channel can be anything between 0-15. Typically reported to the user as 1-16.
// Third parameter is the note number (48 = middle C).
// Fourth parameter is the velocity (64 = normal, 127 = fastest).

void noteOn(byte channel, byte pitch, byte velocity) {
	if (2 > pitch) { return; }
	midiEventPacket_t noteOn = { 0x09, 0x90 | channel, pitch, velocity };
	MidiUSB.sendMIDI(noteOn);
	MidiUSB.flush();

	digitalWrite(LED_BUILTIN, HIGH);
}

void noteOff(byte channel, byte pitch, byte velocity) {
	if (2 > pitch) { return; }

	midiEventPacket_t noteOff = { 0x08, 0x80 | channel, pitch, velocity };
	MidiUSB.sendMIDI(noteOff);
	MidiUSB.flush();

	digitalWrite(LED_BUILTIN, LOW);
}

void makeNoise() {
	if (millis() < last_tone_play_in_millis + sustain) { return; }

	if (playing) {
		switch (playmode) {
		case PLAYMODE_MIDI:
			noteOff(0, last_note, 64);
			break;
		case PLAYMODE_DAC:
			analogWrite(PLAYMODE_DAC_PIN, 0);
			break;
		default:;
		}

		sequence_position = ++sequence_position % sequence_max;
		delay(4);

		switch (playmode) {
		case PLAYMODE_MIDI:
			// midi on the next note
			noteOn(0, scale[sequence[sequence_position]], 64);
			break;
		case PLAYMODE_DAC:
			analogWrite(PLAYMODE_DAC_PIN, map(scale[sequence[sequence_position]], 0, 127, 0, 255));
			break;
		default:;
		}


		last_note = scale[sequence[sequence_position]];
		dirty_player = true;
		last_tone_play_in_millis = millis();
	}
}

static void readKeys() {
	if (millis() < last_button_press_in_millis + DEBOUNCE_DELAY) { return; }

	if (LOW == digitalRead(MARK_PIN)) {
		sequence[sequencer_pointer] = sequence[sequencer_pointer] - 1;
		if (sequence[sequencer_pointer] < 0) { sequence[sequencer_pointer] = scale_max_size - 1; }
		dirty_editor = true;
	}
	if (LOW == digitalRead(RUB_PIN)) {
		sequence[sequencer_pointer] = sequence[sequencer_pointer] + 1;
		if (sequence[sequencer_pointer] >= scale_max_size) { sequence[sequencer_pointer] = 0; }
		dirty_editor = true;
	}

	if (LOW == digitalRead(NOTE_DOWN_PIN)) {
		if (0 == sequencer_pointer) { sequencer_pointer = sequence_max; }
		sequencer_pointer = (sequencer_pointer - 1) % sequence_max;
		dirty_editor = true;
	}

	if (LOW == digitalRead(NOTE_UP_PIN)) {
		if (sequence_max == sequencer_pointer) { sequencer_pointer = 0; }
		sequencer_pointer = (sequencer_pointer + 1) % sequence_max;
		dirty_editor = true;
	}

	sustain = map(analogRead(SUSTAIN_APIN), 0, 1023, max_sustain, 40);

	if (LOW == digitalRead(PLAY_PIN)) {
		playing = !playing;
		dirty_player = true;
		switch (playmode) {
		case PLAYMODE_DAC:
			analogWrite(PLAYMODE_DAC_PIN, 0);
			break;
		case PLAYMODE_MIDI:
			noteOff(0, last_note, 64);
			break;
		}
	}

	last_button_press_in_millis = millis();
}

void drawEditor() {
	lcd.setCursor(0, 0);
	drawSequence(0, 0, sequencer_pointer, sequence_max);
	lcd.setCursor(0, 1);
	lcd.print(sequence[sequencer_pointer]);
	lcd.print("  ");
}

void drawPlayer() {
	lcd.setCursor(4, 1);
	lcd.print(sequence[sequence_position]);
	drawSequence(8, 1, sequence_position % 8, 8);
}

/* update the screen */
void updateScreen() {
	if (millis() < last_lcd_update + REFRESH_DELAY) {
		return;
	}
	if (dirty_editor) {
		drawEditor();
		dirty_editor = false;
	}

	if (dirty_player) {
		drawPlayer();
		dirty_player = false;
	}

	last_lcd_update = millis();
}

void drawSequence(int x, int y, int dot_pos, int length) {
	lcd.setCursor(x, y);
	for (int i = 0; i < length; i++) {
		lcd.write(byte(0xff));
	}
	lcd.setCursor(x + dot_pos, y);
	lcd.write(byte(0b10100101));
}

void loop() {
	readKeys();
	makeNoise();
	updateScreen();
}
