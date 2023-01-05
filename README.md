# C++ Trampoline for the Pi Pico/RP2040

The standard SDK for the Pi Pico uses `irq_handler_t` (`void (*)()`) for interrupt handlers.
Because there is no custom data parameter, there is no obvious way to use a C++ member function for interrupt handling.
The normal solution is to wrap the call through a `static` C function accessing a global variable to get the object pointer.
This seems like an awkward and complicated solution, so I've implemented something that's arguably much worse.

This cursed code generates a trampoline inside the object's data and executes it.
The trampoline itself just fetches the correct `this` pointer and then jumps to the member function.
This is cursed for several reasons:

 - It executes code as data. If you or your RTOS enable XN in the MPU, this will crash.
 - The assembly instructions are hard coded as ARM Thumb opcodes, so it's not portable.
 - There's a race condition lurking in the trampoline if you try to destruct the trampoline object.
   You must guarantee that the trampoline cannot be invoked or in-progress *before* destruction.
   If it's possible for destruction to occur at a time when the triggering event can occur, 
   then you have a race condition problem and are doomed to suffer very rare and undebuggable crashes.

On the other hand:

 - Only 16 bytes per trampoline.
 - Efficient.
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
