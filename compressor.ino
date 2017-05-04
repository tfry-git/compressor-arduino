const int window_ms = 5;  // milliseconds per measurement window. A narrow window will allow finer control over attack and release,
                    // but it will also cripple detection of low frequency amplitudes. Probably you don't want to change this.
int attack_f = 10;  // attack period (how soon the compressor will start attenuating loud signals) given in measurement frame
                    // units. Default setting corresponds to 50ms. Max buf_len.
int release_f = 40; // release period (how soon the compressor will soften attenuation after signals have become more silent),
                    // given in measurement frame units. Default setting corresponds to 200ms; Max buf_len. Should be > attack_f
int threshold = 17; // minimum signal amplitude before the compressor will kick in. Each unit corresponds to roughly 5mV
                    // peak-to-peak.
float dampening = .5;  // dampening applied to signals exceeding the threshold. 0 corresponds to no compression, 1 corresponds
                    // to limiting the signal to the threshold value (if possible: see below)
const int duty_min = 10;  // ceiling value for attenuation (lower values = more attenuation, 0 = off, 255 = no attenuation)
                    // beyond a certain value further attenuation is just too coarse grained for good results. Ideally, this
                    // value is never reached, but might be for aggressive dampening and low thresholds.
const int duty_warn = 2 * duty_min;  // See above. At attenuation beyond this (i.e. smaller numbers), warning LED will flash.
                    // Reaching this point on occasion is quite benign. Reaching this point much of the time means too strong
                    // signal, too low threshold setting, or too aggressive dampening.
const int signal_warn = 300;  // A warning LED will flash for signals exceeding this amplitude (5mv per unit, peak-to-peak) as
                    // it is probably (almost) too much for the circuit too handle (default value corresponds to about +-750mV
                    // in order to stay below typical 2N7000 body diode forward voltage drop of .88V)

volatile int cmin = 1024; // minimum amplitude found in current measurement window
volatile int cmax = 0;    // maximum amplitude found in current measurement window
const int buf_len = 100;  // size of buffer. attack_f and release_f cannot exceed this.
int buf[buf_len];         // ring buffer for moving averages / sums
int pos = 0;              // current buffer position
int attack_mova = 0;      // moving average (actually sum) of amplitudes over past attack period
int release_mova = 0;     // moving average (actually sum) of amplitudes over past release period
int32_t last = 0;         // time of last loop
int duty = 255;           // current attenuation duty cycle (0: hard off, 255: no attenuation)

#define DEBUG 1           // serial communication appear to introduce audible noise ("ticks"), thus diabled by default
#if DEBUG
int it = 0;
#endif

void setup() {
  for (int i = 0; i < buf_len; ++i) {  // clear buffer
    buf[i] = 0;
  }
#if DEBUG
  Serial.begin (9600);
#endif

  // start fast pwm with no prescaler (~62kHz) on pin 3, controlling the attenuator switch(es)
  pinMode(3, OUTPUT);
  TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);      // Prescale factor 1
  OCR2B = 255;             // 100% duty cycle, initially

  // setup fast continuous analog input sampling. Kudos go to https://meettechniek.info/embedded/arduino-analog.html
  DIDR0 = 0x3F;            // digital inputs disabled
  ADMUX = 0b01000000;      // measuring on ADC0, use 5v reference
  ADCSRA = 0xAC;           // AD-converter on, interrupt enabled, prescaler = 16  --> aroudn 77k samples per second
  ADCSRB = 0x40;           // AD channels MUX on, free running mode
  bitWrite(ADCSRA, 6, 1);  // Start the conversion by setting bit 6 (=ADSC) in ADCSRA
  sei();                   // set interrupt flag

  last = millis ();
}

void loop() {
  int32_t now = millis ();
  if (now < last || now - last > window_ms) {  // measurment window elapsed (or timer overflow)
    last = now;
  } else return;

#if DEBUG
  if (++it == 40) {
    it = 0;
    Serial.print (cmax);
    Serial.print ("-");
    Serial.print (cmin);
    Serial.print ("-");
    Serial.println (duty);
  }
#endif

  // get amplitude in current meausrement window, and set up next window
  if (++pos >= buf_len) pos = 0;
  int val = cmax - cmin;
  if (val < 0) val = 0;
  cmax = 0;
  cmin = 1024;

  // update the two moving averages (sums)
  int old_pos = pos - attack_f;
  if (old_pos < 0) old_pos += buf_len;
  attack_mova += val - buf[old_pos];
  old_pos = pos - release_f;
  if (old_pos < 0) old_pos += buf_len;
  release_mova += val - buf[old_pos];

  // store new value in ring buffer
  buf[pos] = val;

  // calculate new attenuation settings
  // first caculate based on attack period
  const int attack_threshold = threshold * attack_f;
  const float attack_overshoot = max (0, (attack_mova - attack_threshold) / (float) attack_threshold);
  const int attack_duty = 255 / (attack_overshoot * dampening + 1);
  // if the new duty setting is _below_ the current, based on attack period, check release window to see, if
  // the time has come to release attenuation, yet:
  if (attack_duty >= duty) duty = attack_duty;
  else {
    const int release_threshold = threshold * release_f;
    const float release_overshoot = max (0, (release_mova - release_threshold) / (float) release_threshold);
    const int release_duty = 255 / (release_overshoot * dampening + 1);
    if (release_duty < duty) duty = release_duty;
  }

  OCR2B = duty; // enable the new duty cycle
}

/*** Interrupt routine ADC ready ***/
ISR(ADC_vect) {
  int aval = ADCL;        // store lower byte ADC
  aval += ADCH << 8;  // store higher bytes ADC
  if (aval < cmin) cmin = aval;
  if (aval > cmax) cmax = aval;
}

