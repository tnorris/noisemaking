/*  Example playing a sinewave at a set frequency,
    using Mozzi sonification library.
  
    Demonstrates the use of Oscil to play a wavetable.
  
    Circuit: Audio output on digital pin 9 on a Uno or similar, or
    DAC/A14 on Teensy 3.1, or 
    check the README or http://sensorium.github.com/Mozzi/
  
    Mozzi help/discussion/announcements:
    https://groups.google.com/forum/#!forum/mozzi-users
  
    Tim Barrass 2012, CC by-nc-sa.
    
    hacked up by Tom Norris
*/

#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <tables/saw2048_int8.h> // sine table for oscillator

bool playing = false;
String inString = "        ";
int inChar = 0;

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SAW2048_NUM_CELLS, AUDIO_RATE> aSin(SAW2048_DATA);

// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 64 // powers of 2 please
int freq = 800;

int STOP_PIN = 7;

void setup(){
  startMozzi(CONTROL_RATE); // set a control rate of 64 (powers of 2 please)
  aSin.setFreq(440); // set the frequency
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
    if (inChar == '\n') {
      freq = inString.toInt();
      inString = "        ";
    }
  }

  aSin.setFreq(freq);
}

int updateAudio(){
  return aSin.next(); // return an int signal centred around 0
}


void loop(){
  audioHook(); // required here
}
