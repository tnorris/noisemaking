/*
   This file owes much of it's life to 
   Tim Barrass 2012+2013, CC by-nc-sa.
   - Example playing a sinewave at a set frequency,
   using Mozzi sonification library.
   - Example applying an ADSR envelope to an audio signal

   Hacked up by Tom Norris
   */

#include <MozziGuts.h>
#include <EventDelay.h>
#include <ADSR.h>
#include <Oscil.h> // oscillator template
#include <tables/saw2048_int8.h>
#include <tables/square_no_alias_2048_int8.h>
#include <tables/sin2048_int8.h>


bool playing = false;
String inString = "        ";
int inChar = 0;
// for triggering the envelope
EventDelay noteDelay;

ADSR <CONTROL_RATE, AUDIO_RATE> envelope;

boolean note_is_on = true;
// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SAW2048_NUM_CELLS, AUDIO_RATE> aOsc(SAW2048_DATA);

// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 64 // powers of 2 please
int freq = 800;

unsigned int attack, decay, sustain, release_ms;
int decay_level = 127;
int attack_level = 255;


int STOP_PIN = 7;

void setup(){
  startMozzi(CONTROL_RATE); // set a control rate of 64 (powers of 2 please)
  aOsc.setFreq(440); // set the frequency
  pinMode(STOP_PIN, INPUT_PULLUP);
  noteDelay.set(2000); // 2 second countdown
  Serial.begin(9600);
  attack = 50;
  sustain = 300;
  decay = 50;
  release_ms = 30;
  envelope.setADLevels(255,127);
  envelope.setTimes(attack,decay,sustain,release_ms);
  envelope.noteOn();
  noteDelay.start(attack+decay+sustain+release_ms);

  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
}

void updateControl(){
  //if(noteDelay.ready()){  noteDelay.start(attack+decay+sustain+release_ms); envelope.noteOn();}
  static int previous;
  int current = digitalRead(STOP_PIN);
  if(previous==LOW && current==HIGH){
    pauseMozzi();
  } else if(previous==HIGH && current==LOW){
    unPauseMozzi();
  }
  previous=current;

  // Read serial input:
  while (Serial.available() > 0) {
    inChar = Serial.read();
    if (isDigit(inChar)) {
      inString += (char)inChar;
    }

    switch(inChar) {
      case '\n':
        freq = inString.toInt();
        inString = "        ";
        envelope.setTimes(attack,decay,sustain,release_ms);
        envelope.noteOn();
        noteDelay.start(attack+decay+sustain+release_ms);
        aOsc.setFreq(freq);
        break;
      case 'S':
        // load the sine wave
        aOsc.setTable(SIN2048_DATA);
        inString = "        ";
        break;
      case 'N':
        // load the saw wave
        aOsc.setTable(SAW2048_DATA);
        inString = "        ";
        break;
      case 'L':
        // load the square wave
        aOsc.setTable(SQUARE_NO_ALIAS_2048_DATA);
        inString = "        ";
        break;
      case 'a':
        attack = inString.toInt();
        inString = "        ";
        break;
      case 'd':
        decay = inString.toInt();
        inString = "        ";
        break;
      case 's':
        sustain = inString.toInt();
        inString = "        ";
        break;
      case 'r':
        release_ms = inString.toInt();
        inString = "        ";
        break;
      case 'A':
        attack_level = inString.toInt();
        inString = "        ";
        envelope.setADLevels(attack_level, decay_level);
        break;
      case 'D':
        decay_level = inString.toInt();
        inString = "        ";
        envelope.setADLevels(attack_level, decay_level);
        break;
    }
  }

  envelope.update();

}

int updateAudio(){
  //return aOsc.next();
  return (int) (envelope.next() * aOsc.next())>>8;
}


void loop(){
  audioHook();
}
