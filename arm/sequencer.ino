#include <frequencyToNote.h>
#include <LiquidCrystal.h>
#include "MIDIUSB.h"  // Arduino DUEs can do this, I think Leonardos can, too
#include "PitchToNote.h"

/* @TODO: shift register */
// moving to the next/prev note in the scale
#define NOTE_UP_PIN   6
#define NOTE_DOWN_PIN 5
#define PLAY_PIN      4

// moving around in the sequence
#define MARK_PIN      3
#define RUB_PIN       2

// 10k pot that determines note sustain
#define SUSTAIN_APIN  0

// 1 ova keyboard
#define NOTE_C_PIN 53
#define NOTE_CS_PIN 51
#define NOTE_D_PIN 49
#define NOTE_DS_PIN 47
#define NOTE_E_PIN 45
#define NOTE_F_PIN 43
#define NOTE_FS_PIN 41
#define NOTE_G_PIN 39
#define NOTE_GS_PIN 37
#define NOTE_A_PIN 35
#define NOTE_AS_PIN 33
#define NOTE_B_PIN 31
int NOTE_PINS[] = { NOTE_C_PIN, NOTE_CS_PIN, NOTE_D_PIN, NOTE_DS_PIN, NOTE_E_PIN, NOTE_F_PIN, NOTE_FS_PIN, NOTE_G_PIN, NOTE_GS_PIN, NOTE_A_PIN, NOTE_AS_PIN, NOTE_B_PIN };

// LCD pins
#define MY_LCD_RS 8
#define MY_LCD_E 9
#define MY_LCD_D7 13
#define MY_LCD_D6 12
#define MY_LCD_D5 11
#define MY_LCD_D4 10

#define REFRESH_DELAY 30

// Yoinked from http://forum.arduino.cc/index.php?topic=79326.0
// Also note the limitations of tone() which at 16mhz specifies a minimum frequency of 31hz - in other words, notes below
// B0 will play at the wrong frequency since the timer can't run that slowly!
uint16_t midi_note_to_frequency[128] PROGMEM = { 8, 9, 9, 10, 10, 11, 12, 12, 13, 14, 15, 15, 16, 17, 18, 19, 21, 22, 23, 24, 26, 28, 29, 31, 33, 35, 37, 39, 41, 44, 46, 49, 52, 55, 58, 62, 65, 69, 73, 78, 82, 87, 92, 98, 104, 110, 117, 123, 131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988, 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976, 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951, 4186, 4435, 4699, 4978, 5274, 5588, 5920, 5920, 6645, 7040, 7459, 7902, 8372, 8870, 9397, 9956, 10548, 11175, 11840, 12544 };


// LiquidCrystal lcd(13, 12, 1, 2, 8, 11);

LiquidCrystal lcd(MY_LCD_RS, MY_LCD_E,
	MY_LCD_D4, MY_LCD_D5, MY_LCD_D6, MY_LCD_D7);

#define DEBOUNCE_DELAY 150

unsigned int sustain = 50;
unsigned int last_sustain = 1;

bool dirty_editor = true;
bool dirty_player = true;

int analog_in = 0;

//int scale[] = { 0, pitchD3, pitchE3, pitchF3, pitchG3, pitchA3, pitchB3b, pitchC4, pitchD4 };
//int scale_max_size = 9;

int scale[] = { 0, pitchC3, pitchD3b, pitchD3, pitchE3b, pitchE3, pitchF3, pitchG3b, pitchG3, pitchA3b, pitchA3, pitchB3b, pitchB3 };
int scale_max_size = 13;

/* int sequence[] = {
	pitchD3, pitchD4, pitchD5, pitchC3,
	pitchC4, pitchC5, pitchC4, pitchD4
}; */

int last_note = 0;


int sequence[] = {
	5, 2, 3, 4,
	1, 11, 3, 6,
	11, 1, 2, 4,
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
int sequencer_pointer = 0;
int last_sequencer_pointer = 0;
int last_lcd_update = 0;

int max_sustain = 1000;

// 0 -> 10
unsigned int volume = 0;

#define PLAYMODE_MIDI 1
#define PLAYMODE_DAC 2
#define PLAYMODE_TTL 4
#define PLAYMODE_DAC_PIN DAC0
int playmode = PLAYMODE_MIDI;

#define DEBUG_DAC_PIN DAC1

bool playing = false;

bool display_numbers = false;

unsigned long last_button_press_in_millis = 0;
unsigned long last_tone_play_in_millis = 0;

void verifyCheatcodes() {
	lcd.setCursor(0, 1);
	if (LOW == digitalRead(MARK_PIN)) {
		sequence_max = 16;
		lcd.print("WIDE");
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

	if (LOW == digitalRead(PLAY_PIN)) {
		lcd.print("NUM!");
		display_numbers = true;
		delay(500);
	}

	if (LOW == digitalRead(NOTE_DOWN_PIN)) {
		playmode = PLAYMODE_TTL;
		lcd.print("TTL!");
		delay(500);
	}
}

void setup() {
	lcd.begin(16, 2);
	lcd.print("Good morning!");
	delay(500);

	pinMode(NOTE_DOWN_PIN, INPUT_PULLUP);
	pinMode(NOTE_UP_PIN, INPUT_PULLUP);
	pinMode(PLAY_PIN, INPUT_PULLUP);
	pinMode(MARK_PIN, INPUT_PULLUP);
	pinMode(RUB_PIN, INPUT_PULLUP);

	pinMode(LED_BUILTIN, OUTPUT);

	for (int i = 0; i < 12; i++) {
		pinMode(NOTE_PINS[i], INPUT_PULLUP);
	}

	last_button_press_in_millis = millis();
	last_tone_play_in_millis = millis();

	volume = 10;

	Serial.begin(9600);

	/* cheatcodes, bruh */
	verifyCheatcodes();
	lcd.clear();
	analogWrite(DEBUG_DAC_PIN, 1023);
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


}

void noteOff(byte channel, byte pitch, byte velocity) {
	if (2 > pitch) { return; }

	midiEventPacket_t noteOff = { 0x08, 0x80 | channel, pitch, velocity };
	MidiUSB.sendMIDI(noteOff);
	MidiUSB.flush();


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
		case PLAYMODE_TTL:
			Serial.print(midi_note_to_frequency[scale[sequence[sequence_position]]]);
			Serial.print('\n');
			delay(2);
			break;

		default:;
		}


		last_note = scale[sequence[sequence_position]];
		dirty_player = true;
		last_tone_play_in_millis = millis();
	}
}

void shiftSequencerPointer(int direction) {
	sequencer_pointer = (sequencer_pointer + direction) % sequence_max;
	if (0 > sequencer_pointer) { 
		sequencer_pointer = sequence_max - 1; 
	} else if (sequence_max == sequencer_pointer) { 
		sequencer_pointer = 0;
	}

	dirty_editor = true;
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
		shiftSequencerPointer(-1);
	}

	if (LOW == digitalRead(NOTE_UP_PIN)) {
		shiftSequencerPointer(1);
	}

	for (int i = 0; i < 12; i++) {
		if (LOW == digitalRead(NOTE_PINS[i])) {
			sequence[sequencer_pointer] = i + 1;
			digitalWrite(LED_BUILTIN, HIGH);
			shiftSequencerPointer(1);
		}
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
	lcd.print("    ");
}

void drawPlayer() {
	lcd.setCursor(4, 1);
	lcd.print(sequence[sequence_position]);
	lcd.print("  ");
	drawSequence(8, 1, sequence_position % 8, 8);
}

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
		if (display_numbers) {
			lcd.print(sequence[i]);
		}
		else {
			lcd.write(byte(0xff));
		}
	}
	lcd.setCursor(x + dot_pos, y);
	lcd.write(byte(0b10100101));
}

void loop() {
	readKeys();
	makeNoise();
	updateScreen();
}
