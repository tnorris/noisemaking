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

#define MODE_PIN 24
#define MODE_LED_A 52
#define MODE_LED_B 50
#define MODE_LED_C 48
#define MODE_LED_D 46
#define MODE_LED_E 44
int LED_PINS[] = { MODE_LED_A, MODE_LED_B, MODE_LED_C, MODE_LED_D, MODE_LED_E };

#define REFRESH_DELAY 30

// Yoinked from http://forum.arduino.cc/index.php?topic=79326.0
// Also note the limitations of tone() which at 16mhz specifies a minimum frequency of 31hz - in other words, notes below
// B0 will play at the wrong frequency since the timer can't run that slowly!
uint16_t midi_note_to_frequency[128] PROGMEM = { 8, 9, 9, 10, 10, 11, 12, 12, 13, 14, 15, 15, 16, 17, 18, 19, 21, 22, 23, 24, 26, 28, 29, 31, 33, 35, 37, 39, 41, 44, 46, 49, 52, 55, 58, 62, 65, 69, 73, 78, 82, 87, 92, 98, 104, 110, 117, 123, 131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988, 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976, 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951, 4186, 4435, 4699, 4978, 5274, 5588, 5920, 5920, 6645, 7040, 7459, 7902, 8372, 8870, 9397, 9956, 10548, 11175, 11840, 12544 };


// LiquidCrystal lcd(13, 12, 1, 2, 8, 11);

LiquidCrystal lcd(MY_LCD_RS, MY_LCD_E,
    MY_LCD_D4, MY_LCD_D5, MY_LCD_D6, MY_LCD_D7);

#define DEBOUNCE_DELAY 50

unsigned int sustain = 500;

bool dirty_editor = true;
bool dirty_player = true;

int analog_in = 0;

//int scale[] = { 0, pitchD3, pitchE3, pitchF3, pitchG3, pitchA3, pitchB3b, pitchC4, pitchD4 };
//int scale_max_size = 9;

int scale[] = { 0, pitchC3, pitchD3b, pitchD3, pitchE3b, pitchE3, pitchF3, pitchG3b, pitchG3, pitchA3b, pitchA3, pitchB3b, pitchB3,
                pitchC4, pitchD4b, pitchD4, pitchE4b, pitchE4, pitchF4, pitchG4b, pitchG4, pitchA4b, pitchA4, pitchB4b, pitchB4 };
int scale_max_size = 13;
int scale_offset = 0;

int last_note = 0;

typedef struct {
    int play_position;
    int max;
    int position;
    int last_position;
    int notes[16];
} Sequencer;

Sequencer s = { 0, 8, 0, 0,
                { 5, 2, 3, 4,
                  1, 11, 3, 6,
                  11, 1, 2, 4,
                  3, 1, 3, 6 }
};

typedef struct {
	unsigned int attack;
	unsigned int sustain;
	unsigned int decay;
	unsigned int release;
	byte attack_level;
	byte decay_level;
} SynthASDRWithLevels;

SynthASDRWithLevels asdr = { 10, 300, 50, 30, 255, 127 };

#define KEYMODE_PIANO 1
#define KEYMODE_CHEATS 2
#define KEYMODE_WAVE 4
#define KEYMODE_ASDR 8
typedef struct {
	bool up;
	bool down;
	bool playpause;
	bool mark;
	bool rub;
	unsigned int mode;
	unsigned int pot;
	bool notes[12];
	bool menu;
} KeyboardState;

KeyboardState k = { false, false, false, false, false, KEYMODE_PIANO, 0 , {false, false, false, false, false, false, false, false, false, false, false, false }, false };
KeyboardState last_k = { false, false, false, false, false, KEYMODE_PIANO, 0 ,{ false, false, false, false, false, false, false, false, false, false, false, false }, false };

typedef struct {
    bool shifty_octaves;
    bool show_bpm;
} CheatCodeFlags;

CheatCodeFlags config = { false, false };

int last_lcd_update = 0;

int max_sustain = 1000;

// 0 -> 10
unsigned int volume = 0;

#define PLAYMODE_MIDI 1
#define PLAYMODE_DAC 2
#define PLAYMODE_TTL 4
#define PLAYMODE_DAC_PIN DAC0
int playmode = PLAYMODE_TTL;


#define DEBUG_DAC_PIN DAC1

bool playing = false;

bool display_numbers = false;

unsigned long last_button_press_in_millis = 0;
unsigned long last_tone_play_in_millis = 0;
unsigned long last_button_repeat_in_millis = 0;

void verifyCheatcodes(int lcd_delay) {
    lcd.setCursor(0, 0);
    if (LOW == k.mark) {
        s.max = 16;
		if (lcd_delay) {
			lcd.print("WIDE");
			delay(lcd_delay);
		}
    }

    if (LOW == k.rub) {
        // midi all off
        midiEventPacket_t noteOn = { 0b10110000, 0b10110000 | 0, 123 };
		if (lcd_delay) {
			lcd.print("SHH!");
			delay(lcd_delay);
		}
    }

    if (LOW == k.up) {
        max_sustain = 4000;
		if (lcd_delay) {
			lcd.print("LONG");
			delay(lcd_delay);
		}
    }

    if (LOW == k.playpause) {
        display_numbers = true;
		if (lcd_delay) {
			lcd.print("NUM!");
			delay(lcd_delay);
		}
    }

    lcd.setCursor(0, 1);
    if (LOW == digitalRead(NOTE_D_PIN)) {
        playmode = PLAYMODE_TTL;
		if (lcd_delay) {
			lcd.print("TTL!");
			delay(lcd_delay);
		}
    }

    if (LOW == digitalRead(NOTE_C_PIN)) {
        config.shifty_octaves = true;
		if (lcd_delay) {
			lcd.print("SHFT");
			delay(lcd_delay);
		}
    }

    if (LOW == digitalRead(NOTE_E_PIN)) {
        config.show_bpm = true;
		if (lcd_delay) {
			lcd.print("BPM!");
			delay(lcd_delay);
		}
    }

	if (LOW == digitalRead(NOTE_F_PIN)) {
		Serial1.print('L');
		if (lcd_delay) {
			lcd.print("SQUR");
			delay(lcd_delay);
		}
	}

	if (LOW == digitalRead(NOTE_G_PIN)) {
		Serial1.print('N');
		if (lcd_delay) {
			lcd.print("SAW!");
			delay(lcd_delay);
		}
	}

	if (LOW == digitalRead(NOTE_A_PIN)) {
		Serial1.print('S');
		if (lcd_delay) {
			lcd.print("SIN!");
			delay(lcd_delay);
		}
	}
}

void setup() {
    lcd.begin(16, 2);


    pinMode(NOTE_DOWN_PIN, INPUT_PULLUP);
    pinMode(NOTE_UP_PIN, INPUT_PULLUP);
    pinMode(PLAY_PIN, INPUT_PULLUP);
    pinMode(MARK_PIN, INPUT_PULLUP);
    pinMode(RUB_PIN, INPUT_PULLUP);
	pinMode(MODE_PIN, INPUT_PULLUP);

    pinMode(LED_BUILTIN, OUTPUT);

    for (int i = 0; i < 12; i++) {
        pinMode(NOTE_PINS[i], INPUT_PULLUP);
    }
	
	for (int i = 0; i < 5; i++) {
		pinMode(LED_PINS[i], OUTPUT);
	}

	digitalWrite(MODE_LED_E, HIGH);

    last_button_press_in_millis = 0;
    last_tone_play_in_millis = 0;

    volume = 10;

    Serial1.begin(9600);

	analogReadResolution(12);
	
	lcd.print("Good morning!");
	delay(500);

	lcd.clear();

    /* cheatcodes, bruh */
	updateKeyboardStruct();
	verifyCheatcodes(500);
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

        s.play_position = ++s.play_position % s.max;
        delay(4);

        switch (playmode) {
        case PLAYMODE_MIDI:
            // midi on the next note
            noteOn(0, scale[s.notes[s.play_position]], 64);
            break;
        case PLAYMODE_DAC:
            analogWrite(PLAYMODE_DAC_PIN, map(scale[s.notes[s.play_position]], 0, 127, 0, 255));
            break;
        case PLAYMODE_TTL:
            Serial1.print(midi_note_to_frequency[scale[s.notes[s.play_position]]]);
            Serial1.print('\n');
			Serial1.flush();
			Serial1.print(asdr.release);
			Serial1.print('r');
			Serial1.flush();
			Serial1.print(asdr.sustain);
			Serial1.print('s');
			Serial1.flush();

			Serial1.print(asdr.decay);
			Serial1.print('d');
			Serial1.flush();

			Serial1.print(asdr.decay_level);
			Serial1.print('D');

			Serial1.flush();

			Serial1.print(asdr.attack);
			Serial1.print('a');

			Serial1.flush();

			Serial1.print(asdr.attack_level);
			Serial1.print('A');


			Serial1.flush();
			delay(2);
            break;
        default:;
        }


        last_note = scale[s.notes[s.play_position]];
        dirty_player = true;
        last_tone_play_in_millis = millis();
    }
}

void shiftSequencerPosition(int direction) {
    s.position = (s.position + direction) % s.max;
    if (0 > s.position) {
        s.position = s.max - 1;
    } else if (s.max == s.position) {
        s.position = 0;
    }

    dirty_editor = true;
}

void readKeys() {
	if (millis() < last_button_press_in_millis + DEBOUNCE_DELAY) { return; }
	last_k = k;

	//lcd.print(k.mode);
	digitalWrite(MODE_LED_B, k.menu);
	updateKeyboardStruct();
	setKeyboardMode();
	
	if (LOW == k.rub && LOW == k.down && LOW == k.playpause) ctrlAltDel();

	switch (k.mode) {
	case KEYMODE_PIANO:
		digitalWrite(MODE_LED_C, HIGH);
		oldReadKeys();
		break;
	case KEYMODE_WAVE:
		digitalWrite(MODE_LED_D, HIGH);
		break;
	case KEYMODE_ASDR:
		digitalWrite(MODE_LED_E, HIGH);
		asdrAssign();
		break;
	default:
		digitalWrite(MODE_LED_E, LOW);

		digitalWrite(MODE_LED_D, LOW);
		digitalWrite(MODE_LED_C, LOW);
	}

	last_button_press_in_millis = millis();
}

void setKeyboardMode() {
	if (LOW == k.menu && changed(k.menu, last_k.menu)) {
		switch (k.mode) {
		case KEYMODE_PIANO:
			k.mode = KEYMODE_ASDR;
			break;
		case KEYMODE_ASDR:
			k.mode = KEYMODE_PIANO;
			break;
		default:
			break;
		}
	}
}

void ctrlAltDel() {
	lcd.setCursor(0, 0);
	lcd.clear();
	lcd.print("Cheats in 1s");
	delay(1000);
	updateKeyboardStruct();
	verifyCheatcodes(500);
	lcd.clear();
	dirty_player = true;
	dirty_editor = true;
	return;
}

void asdrAssign() {
	/*
	typedef struct {
		unsigned int attack;
		unsigned int sustain;
		unsigned int decay;
		unsigned int release;
		byte attack_level;
		byte decay_level;
	} SynthADSRWithLevels;
	*/	
	unsigned int new_val = map(k.pot, 0, 4095, max_sustain, 1);
	if (LOW == k.down) asdr.attack = new_val;
	if (LOW == k.up) asdr.sustain = new_val;
	if (LOW == k.mark) asdr.decay = new_val;
	if (LOW == k.rub) asdr.release = new_val;
}

void updateKeyboardStruct() {
	k.down = digitalRead(NOTE_DOWN_PIN);
	k.up = digitalRead(NOTE_UP_PIN);
	k.mark = digitalRead(MARK_PIN);
	k.rub = digitalRead(RUB_PIN);
	k.playpause = digitalRead(PLAY_PIN);
	k.pot = analogRead(A0);

	k.menu = digitalRead(MODE_PIN);

	for (int i = 0; i < 12; i++) {
		k.notes[i] = digitalRead(NOTE_PINS[i]);
	}
}

bool changed(int a, int b) {
	if (millis() > last_button_repeat_in_millis + (DEBOUNCE_DELAY * 5) ) {
		last_button_repeat_in_millis = millis();
		return true;
	}
	return a != b;
}

static void oldReadKeys() {
	if (LOW == k.mark && changed(k.mark, last_k.mark)) {
        if (config.shifty_octaves) {
            scale_offset = 12;
        } else {
            s.notes[s.position] = s.notes[s.position] - 1;
            if (s.notes[s.position] < 0) { s.notes[s.position] = scale_max_size - 1; }
            dirty_editor = true;
        }
    }
    else {
        if (config.shifty_octaves) scale_offset = 0;
    }

	if (LOW == k.rub && changed(k.rub, last_k.rub)) {
		s.notes[s.position] = s.notes[s.position] + 1;
		if (s.notes[s.position] >= scale_max_size) { s.notes[s.position] = 0; }
		dirty_editor = true;
	}

    if (LOW == k.down && changed(k.down, last_k.down)) {
        shiftSequencerPosition(-1);
    }

    if (LOW == k.up && changed(k.up, last_k.up)) {
        shiftSequencerPosition(1);
    }

    for (int i = 0; i < 12; i++) {
        if (LOW == k.notes[i] && changed(k.notes[i], last_k.notes[i])) {
            s.notes[s.position] = i + 1 + scale_offset;
            digitalWrite(LED_BUILTIN, HIGH);
            shiftSequencerPosition(1);
        }
    }
	sustain = map(k.pot, 0, 4095, max_sustain, 1);

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


}

void drawEditor() {
    lcd.setCursor(0, 0);
    drawSequence(0, 0, s.position, s.max);
    lcd.setCursor(0, 1);
    lcd.print(s.notes[s.position]);
    lcd.print("    ");
}

void drawPlayer() {
    int bpm = 60000 / sustain;
    lcd.setCursor(4, 1);
    if (config.show_bpm) {
        switch (bpm) {
        case 60:
            lcd.print("YAWN");
            break;;
        case 80:
            lcd.print("YAZZ");
            break;;
        case 112:
            lcd.print("JAZZ");
            break;;
        case 120:
            lcd.print("DUB!");
            break;;
        case 130:
            lcd.print("OOHN");
            break;
        case 140:
            lcd.print("TISS");
            break;;
        case 150:
            lcd.print("DNB!");
            break;;
        case 180:
            lcd.print("PSY!");
            break;;
        case 300:
            lcd.print("DDR!");
            break;;
        case 340:
            lcd.print("GABR");
            break;;
        case 1500:
            lcd.print("ARP!");
            break;;
        default:
            lcd.print(bpm);
        }
    }
    else {
        lcd.print(s.notes[s.play_position]);
    }
    lcd.print("  ");
    drawSequence(8, 1, s.play_position % 8, 8);
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
            lcd.print(s.notes[i]);
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
