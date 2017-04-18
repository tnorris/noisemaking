/*
This file owes much of it's life to 
Tim Barrass 2012, CC by-nc-sa.
Example playing a sinewave at a set frequency,
    using Mozzi sonification library.

Hacked up by Tom Norris
 */

#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <tables/saw2048_int8.h> // sine table for oscillator

bool playing = false;
String inString = "        ";
int inChar = 0;

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SAW2048_NUM_CELLS, AUDIO_RATE> aOsc(SAW2048_DATA);

// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 64 // powers of 2 please
int freq = 800;

int STOP_PIN = 7;

void setup(){
  startMozzi(CONTROL_RATE); // set a control rate of 64 (powers of 2 please)
  aOsc.setFreq(440); // set the frequency
  pinMode(STOP_PIN, INPUT_PULLUP);
  Serial.begin(9600);
}

void updateControl(){
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
    // if you get a newline, print the string,
    // then the string's value:
    switch(inChar) {
      case '\n':
        freq = inString.toInt();
        inString = "        ";
        break;
      case 'S':
        // load the sine wave
        break;
      case 'N':
        // load the saw wave
        break;
      case 'L':
        // load the square wave
    }
  }

  aOsc.setFreq(freq);
}

int updateAudio(){
  return aOsc.next(); // return an int signal centred around 0
}


void loop(){
  audioHook();
}
