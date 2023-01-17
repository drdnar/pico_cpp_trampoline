# C++ Trampoline for the Pi Pico/RP2040

The standard SDK for the Pi Pico has several handler/callback APIs that have no provision for a custom data parameter.
Because there is no custom data parameter, there is no obvious way to use a C++ member function for interrupt handling.
The normal solution is to wrap the call through an `extern "C"` function accessing a global variable to get the object pointer.
This seems like an awkward and complicated solution, so I've implemented something that's arguably much worse.

This cursed code generates a trampoline inside the object's data and executes it.
The trampoline itself just fetches the correct `this` pointer and then jumps to the member function.
This is cursed for several reasons:

 - It executes data as code.
   If you or your RTOS enable XN in the MPU, this will crash.
 - It depends on calling convention implementation details.
 - The assembly instructions are hard coded as ARM Thumb opcodes, so it's not portable.
   However, the Pico SDK says to use the normal ARM ABI calling convention and that's what this implements,
   so if your Thumb-based microcontroller's SDK says the same, it should work.
   I haven't checked if this includes the STM32 line.
 - There's a possible race condition lurking in the trampoline if you try to destruct the trampoline object.
   You must guarantee that the trampoline cannot be invoked or in-progress **before** destruction.
   If it's possible for destruction to occur at a time when the triggering event can occur, 
   then you have a race condition problem and are doomed to suffer very rare and undebuggable crashes.

On the other hand:

 - Only 16 or 20 bytes per trampoline.
 - Efficient.
   It might be more efficient than the C adapter method because there's no need to access a global variable.
 - The trampoline awkwardness can be kept entirely (more or less) in the object's header.
 - Suitable for use in header-only libraries.

### Pi Pico

`pico_trampoline.hpp` provides shortcuts for the callbacks specifically used in the Pi Pico SDK.
Specifically, the following are supported:
 - `irq_trampoline` for `irq_handler_t`
 - `exception_trampoline` for `exception_handler_t`
 - `resus_trampoline` for `resus_callback_t`
 - `rtc_trampoline` for `rtc_callback_t`
 - `gpio_irq_trampoline` for `gpio_irq_callback_t`
 - `hardware_alarm_trampoline` for `hardware_alarm_callback_t`
 - `repeating_timer_trampoline` for `repeating_timer_callback_t`
 - `alarm_trampoline` for `alarm_callback_t`

## Examples

### For the Pico SDK
```
#include "pico_trampoline.hpp"
struct pico_example
{
    MAKE_TRAMPOLINE(irq, pico_example, actual_handler, handler); // Macro adds _trampoline to irq for you.
    pico_example()
    {
        irq_set_exclusive_handler(SOME_IRQ_NUMBER, handler);
        do_other_useful_hardware_stuff();
    }
  private:
    void actual_handler()
    {
        handle_irq_stuff();
    }
} example_pico_instance;
// Instantiating example_pico_instance automatically hooks up the interrupt handler for you!
```

### Raw
```
#include "c_trampoline.hpp"
struct example
{
    c_trampoline<example, void> handler { *this, &example::actual_handler };
    example()
    {
        irq_set_exclusive_handler(SOME_IRQ_NUMBER, handler);
        do_other_useful_hardware_stuff();
    }
  private:
    void actual_handler()
    {
        handle_irq_stuff();
    }
} example_instance;
```
