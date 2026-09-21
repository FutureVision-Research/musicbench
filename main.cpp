/*
 * ATtiny85 Touch Activated DFPlayer Controller
 *
 * Target: ATtiny85
 * Clock:  Default 1 MHz internal clock
 * IDE:    Microchip Studio
 * Language: C++
 *
 * Operation:
 *
 * 1. The ATtiny85 enters Power Down sleep mode.
 *
 * 2. Grounding PB3 wakes the ATtiny85.
 *
 * 3. PB4 goes LOW, turning on the P channel NDP6020P MOSFET.
 *
 * 4. The program waits two seconds for the DFPlayer Mini to start.
 *
 * 5. PB0 briefly grounds ADKEY1, causing the DFPlayer Mini to play
 *    the song.
 *
 * 6. The circuit remains powered for 30 minutes.
 *
 * 7. During those 30 minutes, each new touch causes the song to
 *    play again. The contacts must be released before another
 *    touch can trigger the song.
 *
 * 8. After 30 minutes, PB4 goes HIGH, turning off the DFPlayer.
 *
 * 9. The ATtiny85 returns to Power Down sleep mode.
 */

#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdint.h>


/*
 * ATtiny85 pin assignments
 *
 * PB0, physical pin 5:
 * Grounds the DFPlayer ADKEY1 input.
 *
 * PB3, physical pin 2:
 * Receives the signal from the touch circuit.
 * The touch circuit grounds this pin when activated.
 *
 * PB4, physical pin 3:
 * Controls the gate of the NDP6020P P channel MOSFET.
 * LOW turns the DFPlayer power on.
 * HIGH turns the DFPlayer power off.
 */

#define DFPLAYER_TRIGGER_PIN PB0
#define TOUCH_INPUT_PIN      PB3
#define DFPLAYER_POWER_PIN   PB4


/*
 * The main program checks the touch input every 10 milliseconds.
 *
 * Thirty minutes contains:
 *
 * 30 minutes × 60 seconds × 100 checks per second
 * = 180,000 checks
 */

const uint32_t SESSION_CHECK_COUNT = 180000UL;


/*
 * The DFPlayer ADKEY1 input is grounded for 200 milliseconds.
 *
 * PB0 normally remains configured as an input with no internal
 * pullup resistor. This makes it electrically disconnected from
 * ADKEY1 while it is not triggering the song.
 *
 * To trigger the song, PB0 becomes an output and is set LOW.
 * After 200 milliseconds, it returns to input mode.
 */

void triggerSong()
{
    /*
     * Make sure the output register contains a LOW before changing
     * the pin to an output. This prevents a brief HIGH pulse.
     */
    PORTB &= ~(1 << DFPLAYER_TRIGGER_PIN);

    /*
     * Change PB0 to an output. Because its output value is LOW,
     * this grounds the DFPlayer ADKEY1 input.
     */
    DDRB |= (1 << DFPLAYER_TRIGGER_PIN);

    /*
     * Hold ADKEY1 LOW long enough for the DFPlayer to recognize
     * the simulated button press.
     */
    _delay_ms(200);

    /*
     * Return PB0 to input mode. This releases ADKEY1 without
     * applying a HIGH voltage to it.
     */
    DDRB &= ~(1 << DFPLAYER_TRIGGER_PIN);

    /*
     * Keep the internal pullup resistor disabled on PB0.
     */
    PORTB &= ~(1 << DFPLAYER_TRIGGER_PIN);
}


/*
 * Turns on power to the DFPlayer Mini.
 *
 * The NDP6020P is a P channel MOSFET. Pulling its gate LOW turns
 * it on and supplies power to the DFPlayer.
 */

void turnOnDFPlayer()
{
    PORTB &= ~(1 << DFPLAYER_POWER_PIN);
}


/*
 * Turns off power to the DFPlayer Mini.
 *
 * Setting the NDP6020P gate HIGH makes its gate voltage equal to
 * its source voltage. This turns the MOSFET off.
 */

void turnOffDFPlayer()
{
    PORTB |= (1 << DFPLAYER_POWER_PIN);
}


/*
 * Returns true when the touch circuit is active.
 *
 * PB3 uses its internal pullup resistor, so it normally reads HIGH.
 * The touch circuit pulls PB3 LOW when activated.
 */

bool touchIsActive()
{
    return (PINB & (1 << TOUCH_INPUT_PIN)) == 0;
}


/*
 * Places the ATtiny85 into Power Down sleep mode.
 *
 * The pin change interrupt on PB3 remains active and can wake the
 * processor when the touch circuit changes PB3 from HIGH to LOW.
 *
 * A change from LOW to HIGH can also wake the processor. The main
 * program checks the actual pin level after waking and starts a
 * session only when PB3 is LOW.
 */

void enterSleep()
{
    /*
     * Select Power Down mode.
     */
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    /*
     * Prepare the sleep system.
     */
    sleep_enable();

    /*
     * Disable interrupts while checking the input and preparing
     * to sleep. This prevents an interrupt from occurring during
     * the preparation sequence.
     */
    cli();

    /*
     * Clear any pin change interrupt flag that may already be set.
     */
    GIFR |= (1 << PCIF);

    /*
     * Enable interrupts.
     *
     * On an AVR processor, the instruction immediately following
     * sei() executes before an interrupt is serviced. Therefore,
     * the processor enters sleep before responding to the pin
     * change interrupt.
     */
    sei();
    sleep_cpu();

    /*
     * Program execution resumes here after the interrupt wakes
     * the processor.
     */
    sleep_disable();
}


/*
 * Pin change interrupt service routine.
 *
 * No instructions are required here. The interrupt itself wakes
 * the ATtiny85. The main program checks PB3 after waking.
 */

ISR(PCINT0_vect)
{
    /*
     * Intentionally empty.
     */
}


int main()
{
    /*
     * Configure the DFPlayer trigger pin as an input.
     *
     * The internal pullup resistor remains disabled so this pin
     * does not apply voltage to ADKEY1.
     */
    DDRB &= ~(1 << DFPLAYER_TRIGGER_PIN);
    PORTB &= ~(1 << DFPLAYER_TRIGGER_PIN);

    /*
     * Configure PB3 as the touch circuit input.
     */
    DDRB &= ~(1 << TOUCH_INPUT_PIN);

    /*
     * Enable the internal pullup resistor on PB3.
     *
     * PB3 will normally read HIGH. The touch circuit must pull
     * PB3 LOW when it is activated.
     */
    PORTB |= (1 << TOUCH_INPUT_PIN);

    /*
     * Put a HIGH value into the PB4 output register before making
     * PB4 an output. This prevents a brief LOW pulse that could
     * turn on the DFPlayer during startup.
     */
    PORTB |= (1 << DFPLAYER_POWER_PIN);

    /*
     * Configure PB4 as an output.
     *
     * Since PB4 is already HIGH, the NDP6020P and DFPlayer remain
     * turned off.
     */
    DDRB |= (1 << DFPLAYER_POWER_PIN);

    /*
     * Enable the pin change interrupt system.
     */
    GIMSK |= (1 << PCIE);

    /*
     * Enable a pin change interrupt specifically for PB3.
     */
    PCMSK |= (1 << PCINT3);

    /*
     * Enable interrupts globally.
     */
    sei();

    /*
     * Repeat forever.
     */
    while (true)
    {
        /*
         * Make certain the DFPlayer is off before sleeping.
         */
        turnOffDFPlayer();

        /*
         * Enter Power Down mode and wait for PB3 to change.
         */
        enterSleep();

        /*
         * A pin change interrupt occurs both when PB3 goes LOW and
         * when it goes HIGH. Wait briefly to reduce contact bounce
         * and electrical noise.
         */
        _delay_ms(20);

        /*
         * Start a new session only if PB3 is still LOW after the
         * debounce delay.
         *
         * If PB3 is HIGH, the processor may have awakened because
         * the contacts were released. In that case, the loop
         * returns the processor to sleep.
         */
        if (!touchIsActive())
        {
            continue;
        }

        /*
         * Turn on power to the DFPlayer Mini.
         */
        turnOnDFPlayer();

        /*
         * Wait two seconds for the DFPlayer Mini to initialize
         * and read its microSD card.
         */
        _delay_ms(2000);

        /*
         * Trigger the song for the touch that started this
         * 30 minute session.
         */
        triggerSong();

        /*
         * Remember that the contacts are presently active.
         *
         * Another song cannot be triggered until the contacts
         * have first been released.
         */
        bool touchWasActive = true;

        /*
         * Keep the DFPlayer powered for approximately 30 minutes.
         */
        for (uint32_t checkNumber = 0;
             checkNumber < SESSION_CHECK_COUNT;
             checkNumber++)
        {
            /*
             * Read the current condition of the touch input.
             */
            bool touchActiveNow = touchIsActive();

            /*
             * Trigger the song only when the touch input changes
             * from released to active.
             *
             * This prevents a person who continues holding the
             * contacts from repeatedly starting the song.
             */
            if (touchActiveNow && !touchWasActive)
            {
                /*
                 * Wait briefly and check the input again to reject
                 * contact bounce or short noise pulses.
                 */
                _delay_ms(20);

                if (touchIsActive())
                {
                    triggerSong();
                    touchActiveNow = true;
                }
            }

            /*
             * Save the current touch condition for comparison on
             * the next pass through the loop.
             */
            touchWasActive = touchActiveNow;

            /*
             * Wait before checking the input again.
             */
            _delay_ms(10);
        }

        /*
         * The 30 minute active period is complete.
         *
         * Turn off the DFPlayer. The main loop will then put the
         * ATtiny85 back into Power Down sleep mode.
         */
        turnOffDFPlayer();
    }
}