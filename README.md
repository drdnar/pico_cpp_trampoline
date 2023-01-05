# C++ Trampoline for the Pi Pico/RP2040

The standard SDK for the Pi Pico uses `irq_handler_t` (`void (*)()`) for interrupt handlers.
Because there is no custom data parameter, there is no obvious way to use a C++ member function for interrupt handling.
The normal solution is to wrap the call through an `extern "C"` function accessing a global variable to get the object pointer.
This seems like an awkward and complicated solution, so I've implemented something that's arguably much worse.

This cursed code generates a trampoline inside the object's data and executes it.
The trampoline itself just fetches the correct `this` pointer and then jumps to the member function.
This is cursed for several reasons:

 - It executes data as code.
   If you or your RTOS enable XN in the MPU, this will crash.
 - It depends on the compiler using `r0` for `this`.
   This is typical, but not actually required.
 - The assembly instructions are hard coded as ARM Thumb opcodes, so it's not portable.
   However, the Pico SDK says to use the normal ARM ABI calling convention and that's what this implements,
   so if your Thumb-based microcontroller's SDK says the same, it should work.
   I haven't check if this includes the STM32 line.
 - There's a race condition lurking in the trampoline if you try to destruct the trampoline object.
   You must guarantee that the trampoline cannot be invoked or in-progress **before** destruction.
   If it's possible for destruction to occur at a time when the triggering event can occur, 
   then you have a race condition problem and are doomed to suffer very rare and undebuggable crashes.

On the other hand:

 - Only 16 bytes per trampoline.
 - Efficient.
   It might be more efficient than the C adapter method because there's no need to access a global variable.
 - The trampoline awkwardness can be kept entirely (more or less) in the object's header.
 - Suitable for use in header-only libraries.

## Example:

```
struct example
{
    example()
    {
        irq_set_exclusive_handler(SOME_IRQ_NUMBER, example_instance.handler);
        do_other_useful_hardware_stuff();
    }
    c_trampoline<example> handler { *this, &example::actual_handler };
  private:
    void actual_handler()
    {
        handle_irq_stuff();
    }
} example_instance;
// Instantiating example automatically hooks up the interrupt handler for you!
```
