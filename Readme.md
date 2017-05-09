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

Let's start with the bottom half of the circuit: Here, the audio signal is decoupled via a largish capacitor, and biased to a level of roughly 3.3V, in order to bring it to a
level suitable for sampling by the arduino. The reason to use 3.3V, here, is simply that a stabilized 3.3V output is already available on many Arduino boards. In principle we could
simply use two resistors to form a voltage divider (and probably base that on 2.5V), but a stabilized voltage is clearly preferrable, esp. when powering from USB. Contrary to the schematic, an
electrolyte cap is perfectly fine for the decoupling, with the negative side connected to audio in, and the positive side to the Arduino side. The capacitor should be rather large in order to
allow for low frequencies to pass. However, the exact value does not matter. Similarly, the resistor value does not matter too much at all.

The biased signal is now fed into the Arduino (pin A0), where it will be sampled at roughly 77kHz. The sampling accuracy is not terribly good, and so one of the requirements of this basic circuit is
having a suitably large input signal (line level/headphone level is more than enough). But not a whole lot of accuracy is needed, either: The only point of this part of the circuit is to allow
the Arduino to detect the current signal level, and - remember - low signal levels are not of interest in the first place.

The more interesting part of the circuit is the upper half. Two N-FETs are connected back to back (drain of the first connected to source of the second and vice versa), which are both controlled
synchronously from Arduino pin D3. These two FETs simply function as a _simple_ analog switch. The good news is that you are pretty likely to have those N-FETs in your part-collection already:
A pair of 2N7000 or any other common small signal N-FET will do ok. Importantly, the FET should be _far_ inside the on-region at 4-5 Volts. Also, of course, if should be able to handle whatever current
will be flowing. A somewhat better choice than 2N7000 would be IRLML2502, and an even better choice will be to use a dedicated analog switch (in this particular schematic
you'd want one supporting negative voltage swings!), but again, the 2N7000 will perform ok-ish, and is enough for connecting a headphone, so try that, first. Also, again, the exact resistor values will
not matter. The 220 Ohms is for limiting the gate (dis-)charge current, to what the Arduino can safely handle. The 470 Ohms is to provide a bit of isolation from noise in the power supply.

What the switch is doing is simply turning on and off the audio ground[1] at a rate and duty cycle controlled by Arduino digital pin 3 (PWM). The code uses a ~66kHz frequency with duty cycle adjusted
between 100% and ~5%. The 66kHz switching will not be audible (your speakers will not even be able to represent such frequencies), but if you are concerned about high frequency artifacts, you could
easily add a simple low-pass filter.

That's all folks, nothing else needed. At least if all you need is switching a line level / headphone level mono signal. Need more? Read on / hang on for more sophisticated circuits built around
the same idea.

## Adding status indicators and parameter control

With some luck, the above circuit will simply work for you, but in many cases you will have to do some tuning. In order to do that, easily, let's add some status indicators and contols:

- Connect four LEDs (with appropriate resistors) from pins D10 through D13 to ground. I suggest using green on D10, yellow and D11 and red on D12 and D13. 
- Connect a 2 by 4 button matrix to pins D4 through D9. D8 and D9 should be connected to the two row wires, D4 through D7 should be connected to the four column wires.

The two buttons at D4 will be used for tuning attack up and down, the buttons at D5 are for controlling release, D6 is threshold, D7 is ratio. (Pin setup can be customized in the source).

## Tuning the compressor parameters

Ok, so how to get started?
1. First, slowly turn up the input volume such that the LED on pin D13 will _almost_, but not quite light up on the loudest signals. (This LED is meant to signify sound levels that are approaching the
   limits of what the hardware can handle. Don't worry if your signal source does not deliver that much. In this case just turn up the input signal as much as possible).
2. Next adjust the threshold value. The green LED on pin D10 will light up as soon as the compressor starts kicking in, i.e. once the threshold is reached. The threshold should be high enough that
   you can comfortably hear important sounds (such as low speech), but ideally a good deal below sounds that are "definitely too loud". Too low of a threshold may lead to sound artifacts esp. for low signals.
3. Adjust the ratio such that the loudest sounds remain inside the acceptable range. Generally, the red LED at D12 should not light up, or only for extreme sounds. It indicates that the compressor
   is operating close to the limit. Much louder, and sounds cannot be tuned down without considerable distortion. (D12 corresponds to tuning down the signal to roughly 1/12 or -22dB; the maximum the
   compressor will do is -28dB; the yellow LED at D11 signals tuning down to 1/2 or -6dB).
4. For adjusting attack and release, it's hardest to outline a clear procedure, but also these will generally work quite fine at their default values. Note that too small values of attack can lead
   to artifacts, too small values of release can lead to the compressor "pumping" on certain sounds.

## More to come

... but writing this up is more work than it may seem. If you find this useful, consider donating a bread crumb or two via Paypal: thomas.friedrichsmeier@gmx.de


## Footnotes

[1] Originally, I was switch the audio signal line, instead of audio ground. Switching on the ground connection has two advantages: First, the N-FETs gate-to-source potential will be independent of the
current signal level, allowing for more linearity. Second, this way subject to some caveats you can switch on and off a stereo signal with a single N-FET pair (but alternatively, connect a second pair in parallel,
also connected to pin D3). Of course, if your next stage is e.g. a high impedance amp, rather than a speaker coil, you should bias "Audio Ground Out" to ground via a largish resistor (some k Ohms).
