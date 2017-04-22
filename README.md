# noisemaking
I'm getting my feet wet with microcontrollers

## Sequencer
- based off of an Arduino Due.

## Synthesizer
- based off of an Arduino Uno

## Troubleshooting
Aka documented toe stubbing.
### There's a high pitched whine coming from the ARM
- Add a Logic Level Converter - Bi-Directional <https://www.sparkfun.com/products/12009> to the serial bus. AVR speaks 5v, ARM speaks 3.3v

### Cheat Codes
When booting, hold down key chords to enter cheat codes:
- Mark: `WIDE` Doubles the amount of notes in the sequencer
- Rub: `SSH!` Sends MIDI All Off
- Up: `LONG` Bumps max sustain to 4 seconds
- Play: `NUM!` Shows scale number instead of squares
- D: `TTL!` Send data to sequencer over the Arduino's TTL lines
- C: `SHFT` Hold down mark while pressing a note, it will be an octave higher
- E: `BPM!` Show sustain's BPM on the LCD
