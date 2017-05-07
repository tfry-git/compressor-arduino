//// main compressor parameters. Adjust these to your needs. ////
int attack_f = 10;  // attack period (how soon the compressor will start attenuating loud signals) given in measurement frame
                    // units (see window_ms). Default setting corresponds to 50ms. Max buf_len / 2. Min 4.
int release_f = 40; // release period (how soon the compressor will soften attenuation after signals have become more silent),
                    // given in measurement frame units. Default setting corresponds to 200ms; Max buf_len.
                    // Does not have an effect if <= attack_f
int threshold = 20; // minimum signal amplitude before the compressor will kick in. Each unit corresponds to roughly 5mV
                    // peak-to-peak.
float inv_ratio = .5;  // dampening applied to signals exceeding the threshold. 0 corresponds to no compression, 1 corresponds
                    // to limiting the signal to the threshold value (if possible: see below)
                    // (Inverse of "ratio" parameter of typical compressors)

//// Some further constants that you will probably not have to tweak ////
#define DEBUG 1           // serial communication appears to introduce audible noise ("ticks"), thus debugging is diabled by default
const int window_ms = 5;  // milliseconds per measurement window. A narrow window will allow finer control over attack and release,
                    // but it will also cripple detection of low frequency amplitudes. Probably you don't want to change this.
const int buf_len = 100;  // size of buffer. attack_f and release_f cannot exceed this.
const int duty_min = 10;  // ceiling value for attenuation (lower values = more attenuation, 0 = off, 255 = no attenuation)
                    // beyond a certain value further attenuation is just too coarse grained for good results. Ideally, this
                    // value is never reached, but might be for aggressive dampening ratio and low thresholds.
const int duty_warn = 2 * duty_min;  // See above. At attenuation beyond this (i.e. smaller numbers), warning LED will flash.
                    // Reaching this point on occasion is quite benign. Reaching this point much of the time means too strong
                    // signal, too low threshold setting, or too aggressive inv_ratio.
const int signal_warn = 300;  // A warning LED will flash for signals exceeding this amplitude (5mv per unit, peak-to-peak) as
                    // it is probably (almost) too much for the circuit too handle (default value corresponds to about +-750mV
                    // in order to stay below typical 2N7000 body diode forward voltage drop of .88V)

//// Adjustable pin assignments
const int pin_led_warn = 13;
const int pin_led_high = 12;
const int pin_led_mid = 11;
const int pin_led_low = 10;

const int pin_attack = 4;
const int pin_release = 5;
const int pin_threshold = 6;
const int pin_ratio = 7;
const int pin_control_plus = 8;
const int pin_control_minus = 9;

//// working variables ////
volatile int cmin = 1024; // minimum amplitude found in current measurement window
volatile int cmax = 0;    // maximum amplitude found in current measurement window
int buf[buf_len];         // ring buffer for moving averages / sums
int pos = 0;              // current buffer position
int attack_mova = 0;      // moving average (actually sum) of amplitudes over past attack period
int release_mova = 0;     // moving average (actually sum) of amplitudes over past release period
int32_t now = 0;          // start time of current loop
int32_t last = 0;         // time of last loop
int duty = 255;           // current PWM duty cycle for attenuator switch(es) (0: hard off, 255: no attenuation)
byte display_hold = 0;

#if DEBUG
int it = 0;
#endif

/*** Handle new analog readings as they become available. This simply records the highest and lowest voltages seen in the current
     measurement window. All real (and more computation-heavy) handling is done inside loop(). ***/
ISR(ADC_vect) {
  int aval = ADCL;    // store lower byte ADC
  aval += ADCH << 8;  // store higher byte ADC
  if (aval < cmin) cmin = aval;
  if (aval > cmax) cmax = aval;
}

void setup() {
  for (int i = 0; i < buf_len; ++i) {  // clear buffer
    buf[i] = 0;
  }
#if DEBUG
  Serial.begin(9600);
#endif

  // start fast pwm with no prescaler (~62kHz) on pin 3, controlling the attenuator switch(es)
  pinMode(3, OUTPUT);
  TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);      // Prescale factor 1
  OCR2B = 255;             // 100% duty cycle, initially

  // setup fast continuous analog input sampling. Kudos go to https://meettechniek.info/embedded/arduino-analog.html
  // whenever a new reading is available, the routine defined by ISR(ADC_vect) is called.
  DIDR0 = 0x3F;            // digital input buffers disabled on all analog pins
  ADMUX = 0b01000000;      // measuring on ADC0, use 5v reference
  ADCSRA = 0xAC;           // AD-converter on, interrupt enabled, prescaler = 16  --> around 77k samples per second
  ADCSRB = 0x40;           // AD channels MUX on, free running mode
  bitWrite(ADCSRA, 6, 1);  // Start the conversion by setting bit 6 (=ADSC) in ADCSRA
  sei();                   // set interrupt flag

  last = millis();

  // status display
  pinMode(pin_led_low, OUTPUT);
  pinMode(pin_led_mid, OUTPUT);
  pinMode(pin_led_high, OUTPUT);
  pinMode(pin_led_warn, OUTPUT);

  // control buttons. Set up for attaching an 4*2 (or larger) button matrix 
  pinMode(pin_control_plus, OUTPUT);
  pinMode(pin_control_minus, OUTPUT);
  pinMode(pin_attack, INPUT_PULLUP);
  pinMode(pin_release, INPUT_PULLUP);
  pinMode(pin_threshold, INPUT_PULLUP);
  pinMode(pin_ratio, INPUT_PULLUP);
}

void loop() {
  now = millis();
  if (now < last || now - last > window_ms) {  // measurment window elapsed (or timer overflow)
    last = now;
  } else return;

#if DEBUG
  if (++it == 40) {
    it = 0;
    Serial.print(cmax);
    Serial.print("-");
    Serial.print(cmin);
    Serial.print("-");
    Serial.println(duty);
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
  const int attack_duty = 255 / (attack_overshoot * inv_ratio + 1);
  // if the new duty setting is _below_ the current, based on attack period, check release window to see, if
  // the time has come to release attenuation, yet:
  if (attack_duty >= duty) duty = attack_duty;
  else {
    const int release_threshold = threshold * release_f;
    const float release_overshoot = max (0, (release_mova - release_threshold) / (float) release_threshold);
    const int release_duty = 255 / (release_overshoot * inv_ratio + 1);
    if (release_duty < duty) duty = release_duty;
  }

  OCR2B = duty; // enable the new duty cycle

  if ((display_hold < 90) && handleControls()) { // check state of control buttons. If any was pressed, the status LEDs shall not be
                        // updated for the next half second (they will indicate control status, instead)
    display_hold = 100;
  }
  if (display_hold) {
    --display_hold;
  } else {
    indicateLevels(val, duty);
  }
}

// query matrix of control buttons and handle any presses. Returns true, if something was changed.
bool handleControls () {
  digitalWrite(pin_control_plus, LOW);
  digitalWrite(pin_control_minus, HIGH);
  if (!digitalRead(pin_attack)) {
    attack_f = min(buf_len/2, attack_f + 1);
    indicateControls(attack_f, 4, buf_len / 2);
    return true;
  }
  if (!digitalRead(pin_release)) {
    release_f = min(buf_len, release_f + 2);
    indicateControls(release_f, 4, buf_len);
    return true;
  }
  if (!digitalRead(pin_threshold)) {
    threshold = min(signal_warn / 2, min(threshold+1, (int) threshold*1.1));
    indicateControls(threshold, 0, signal_warn / 2);
    return true;
  }
  if (!digitalRead(pin_ratio)) {
    inv_ratio = min(1.0, inv_ratio + .05);
    indicateControls(inv_ratio*100, 0, 100);
    return true;
  }
  digitalWrite(pin_control_minus, LOW);
  digitalWrite(pin_control_plus, HIGH);
  if (!digitalRead(pin_attack)) {
    attack_f = max(4, attack_f-1);
    indicateControls(attack_f, 4, buf_len / 2);
    return true;
  }
  if (!digitalRead(pin_release)) {
    release_f = max(4, release_f - 2);
    indicateControls(release_f, 4, buf_len);
    return true;
  }
  if (!digitalRead(pin_threshold)) {
    threshold = max(0, min(threshold-1, (int) threshold/1.1));
    indicateControls(threshold, 0, signal_warn / 2);
    return true;
  }
  if (!digitalRead(pin_ratio)) {
    inv_ratio = max(0, inv_ratio - .05);
    indicateControls(inv_ratio*100, 0, 100);
    return true;
  }
  return false;
}

void indicateControls (int value, int minv, int maxv) {
  digitalWrite(pin_led_warn, (value <= minv) || (value >= maxv)); // Use warning LED to signal either end of scale reached
  // NOTE: Intentionally "reversing" the LED scale, here, to make it easier to differentiate from signal level indication
  float rate = (float) value / (maxv-minv);
  digitalWrite(pin_led_high, rate > .25);
  digitalWrite(pin_led_mid, rate > .5);
  digitalWrite(pin_led_low, rate > .75);
}

void indicateLevels (int rawval, int cduty) {
  digitalWrite (pin_led_warn, rawval >= signal_warn);
  digitalWrite (pin_led_high, cduty <= duty_warn);
  digitalWrite (pin_led_mid, cduty < 128);
  digitalWrite (pin_led_low, cduty != 255);
}

