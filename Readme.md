# Very low part count audio compressor based on Arduino

This project aims to provide a simple to build audio-compressor based on an arduino / genuino.

## A small bit of theory

If you came here, you probably know, what an audio-compressor is used for: Reducing the dynamic range of an audio
signal, often so the more subtle sounds are not affected, but the louders sounds are brought down to levels compatible
with your hardware, ears, and/or neighbors.

Achieving a useful compression without undesirable side-effects depends on careful tuning of some parameters, though:
- Attack: Onset response time, i.e. how soon the compressor starts toning down. Should be fast, but not _too_ fast. You want to cut out loudness, not crispiness.
- Release: Offset response time. Usually several times longer than attack, in oder to avoid oscillation.
- Threshold: Signals below a certain level should not be compressed.
- Ratio: Proportion by which to tune down signals exceeding the threshold, e.g. 2, in order to cut excess in half.

Sounds like a perfect job for a microcontroller, right?

Beyond the logic and timing, however, there are two main technical difficulties: First, we need to sense the signal level to work on. Ok, not terribly difficult,
but does require some tricks to achieve with an acceptable sampling rate on a microcontroller. Second, we need to modify (or output) the signal, accordingly. Typical
approaches involve voltage controlled amplifiers (VCAs), J-FETs, or some other technique to transform a voltage signal into a variable resistance. Well-established,
but not quite trivial, if you read up on it. Instead of these, I decided to capitalize on the main advantage of a microprocessor: Doing simple things _fast_. In this
case this means switch the audio signal on and off at a rate much higher than audible sounds, in order to achieve a variable resistance very easily.

## A first basic version

The following circuit is far from ideal in many ways, but don't worry, it's easy to improve on (even without changing the microcontroller code, for the most part).
But we'll start simple (and quite possibly, this may already be enough for your prupose):

![Basic mono schematic](https://github.com/tfry-git/compressor-arduino/raw/master/doc/basic_mono_circuit.png)

Let's start with the bottom half of the circuit: Here, the audio signal is decoupled via a largish capacitor, and biased to a level of roughly 2.5V, created by theory
voltage divder formed from the two 1k resistors. The whole point is to convert the signal to a level suitable for sampling by the arduino. Contrary to the schematic, an
electrolyte cap is perfectly fine, here, with the negative side connected to ground. It should be rather large in order to allow for low frequencies to pass. However, the exact
value does not matter. Similarly, the resistor values do not matter too much at all.

The biased signal is now fed into the Arduino (pin A0), where it will be sampled at roughly 77kHz. The sampling accuracy is not terribly good, and so one of the requirements of this basic circuit is
having a suitably large input signal (line level/headphone level is more than enough). But not a whole lot of accuracy is needed, either: The only point of this part of the circuit is to allow
the Arduino to detect the current signal level, and - remember - low signal levels are not of interest in the first place.

The more interesting part of the circuit is the upper half. Two N-FETs are connected back to back (D of the first connected to S of the second and vice versa), which are both controlled
synchronously from Arduino pin D3. These two FETs simply function as a _simple_ analog switch. The good news is that you are pretty likely to have those N-FETs in your part-collection already:
A pair of 2N7000 or any other common small signal N-FET will do ok. Importantly, the FET should be _far_ inside the on-region at 4-5 Volts. Also, preferrably, the forward-voltage drop of the
body diode should be as large as possible. A somewhat better choice than 2N7000 would be IRLML2502, and an even better choice will be to use a dedicated analog switch (in this particular schematic
you'd want one supporting negative voltage swings!), but again, the 2N7000 will perform ok-ish, so try that, first.

What the switch is doing is simply turning on and off the audio signal at a rate and duty cycle controlled by Arduino digital pin 3 (PWM). The code uses a ~66kHz frequency with duty cycle adjusted
between 100% and ~5%. The 66kHz switching will not be audible (your speakers will not even be able to represent such frequencies), but if you are concerned about high frequency artifact, you could
easily add a simple low-pass filter.

That's all folks, nothing else needed. At least if all you need is switching a line level / headphone level mono signal. Need more? Read on / hang on for more sophisticated circuits built around
the same idea.

## Adding status indicators and parameter control

- Acutally already implemented in the sketch. Try reading the comments, there.

## More to come

... but writing this up is more work than it may seem. If you find this useful, consider donating a bread crumb or two via Paypal: thomas.friedrichsmeier@gmx.de
