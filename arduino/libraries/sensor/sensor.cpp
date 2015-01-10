/**
 * Group number: 28
 * Student 1:
 * Jente Hidskes, 4335732
 * Student 2:
 * Piet van Agtmaal, 4321278
 */

#include "sensor.h"
#include "engine.h"
#include "Arduino.h"

/**
 * From the HC-SR04 datasheet
 * (http://www.electroschematics.com/wp-content/uploads/2013/07/HC-SR04-datasheet-version-2.pdf):
 *
 * A short ultrasonic pulse is transmitted at the time 0, reflected by an
 * object. The sensor receives this signal and converts it to an electric
 * signal. The next pulse can be transmitted when the echo is faded away.
 * This time period is called cycle period. The recommend cycle period
 * should be no less than 50ms. If a 10μs width trigger pulse is sent to 
 * the signal pin, the Ultrasonic module will output eight 40kHz ultrasonic
 * signal and detect the echo back. The measured distance (cm) is proportional
 * to the echo pulse width and can be calculated by (pulse width / 58). If
 * no obstacle is detected, the output pin will give a 38ms high level signal.
 */

Sensor::Sensor(Engine *engine, int trigger, int echo) : engine(engine), trigger(trigger), echo(echo) {
	pinMode(trigger, OUTPUT);
	pinMode(echo, INPUT);

	/* Configure Timer1 to reliably generate a pulse on 250ms intervals.
	 *
	 * To do so, our value in the compare match register must be:
	 * (16000000 / (64 * 4)) - 1 = 62499, where 16000000 is the Arduino
	 * that runs at 16MHz, 64 is our prescaler value (smaller is more accurate)
	 * and 4Hz means 250 ms.
	 *
	 * We configure Timer1 because it is the first available one:
	 * Timer0,  8 bit: too small and micros(), millis() and delay() use this;
	 * Timer1, 16 bit: large enough, free.
	 * Timer2,  8 bit: too small, free.
	 * Timer3, 16 bit: large enough, but PWM uses this.
	 * Timer4, 16 bit: large enough, but PWM uses this.
	 * Timer5, 16 bit: large enough, free.
	 */

	/* Disable interrupts while we initiate the timer. */
	noInterrupts();

	/* Clear entire Timer Counter Control Register 1A. */
	TCCR1A = 0;
	/* Clear entire Timer Counter Control Register 1B. */
	TCCR1B = 0;
	/* Initialize Timer Count 1 to 0. */
	TCNT1  = 0;

	/* Set the On Compare Reset 1A register value for 4Hz increments, see comment
	 * above for the formula behind the magic number. */
	OCR1A = 62499;
	/* Enable Clear Timer on Compare match mode by setting the correct Wave Generation Modus. */
	TCCR1B |= (1 << WGM12);
	/* Set 64 prescaler by setting the correct Clock Select bits. */
	TCCR1B |= (1 << CS10) | (1 << CS11);
	/* Timer 1 Interrupt Mask Register: set On Compare Interrupt Enable 1A. */
	TIMSK1 |= (1 << OCIE1A);

	/* Reenable interrupts. */
	interrupts();
}

ISR(TIMER1_COMPA_vect) {
	if (sensor) {
		sensor->generatePulse();
	}
}

void Sensor::generatePulse() {
	/* Generate a pulse. */
	digitalWrite(this->trigger, HIGH);
	// FIXME: check if this isn't too long for a callback.
	delayMicroseconds(MIN_PULSE_LENGTH);
	digitalWrite(this->trigger, LOW);
	// FIXME: do check if this implicitly sets ECHO to HIGH!
}

unsigned long Sensor::receivePulse() {
	/* Upper bound on micros(): if micros() is bigger than this value,
	 * the returned distance will be larger than what we need it to be
	 * so it's not worth waiting for anymore. */
	unsigned long max_time = micros() + MAX_DELAY;

	/* Block while the ECHO pin is HIGH. */
	while (digitalRead(this->echo) == HIGH) {
		/* If we cros our upper bound, return. */
		if (micros() > max_time) {
			return 0;
		}
	}

	// FIXME: not sure if needed or if implicitly done.
	//digitalWrite(this->echo, LOW);
	/* Return the duration of the ECHO pin being HIGH. */
	return micros() - (max_time - MAX_DELAY); //FIXME: also subtract instruction overhead?
}

void Sensor::poll() {
	unsigned long duration, distance;

	if (!(duration = receivePulse())) {
		return;
	}

	distance = duration / PULSE_DIVIDE;
	if (distance < MAX_DISTANCE) {
		engine.stop();
	} else if (engine.isStopped()) {
		engine.start();
	}
}
