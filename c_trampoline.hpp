#ifndef C_TRAMPOLINE_H
#define C_TRAMPOLINE_H

/**
 * @brief Adapts a non-static class method for use with a no-arguments C callback.
 * 
 * @note There is no getter/setting for the object reference.
 * Instead, the intended usage is that the object will have this as a member and then, if needed,
 * the object uses the getter/setters for method.
 * 
 * @warning This uses Thumb ARM opcodes and depends on data being executable.
 * If you aren't in ARMv6 Thumb or have XN set in the MPU (memory protection unit), this will fail.
 * 
 * @warning Disabling this properly in a multithreaded environment is non-trivial.
 * There's a possibility that this is destroyed while the asm_code block is executing and
 * if that happens you will not have a fun time debugging the issue.
 * You have to know ahead of time exactly what circumstances can trigger the callback
 * and then make sure they don't happen during destruction.
 * 
 * The destructor here is not virtual and making it virtual may cause Other Issues, so you'll
 * have to use composition to bolt a custom destructor on if that's needed, although that's
 * possibly properly the containing object's job.
 * 
 * @tparam T The class type is required because this adapts a pointer-to-method, which is class-specific.
 */
template <typename T>
struct __attribute__((__packed__)) c_trampoline
{
        /**
         * @brief Type of a pointer-to-member function.
         * 
         * Required for syntax reasons.
         */
        typedef void (T::*this_callback_void)();
        /**
         * @brief Type of a function pointer.
         * 
         * Also used for syntax reasons.
         */
        typedef void (*c_callback_void)();
        
        /**
         * @brief Only reasonable constructor.
         * 
         * @param self Object to bind this wrapper to.
         * @param method Member function pointer you want adapted for use with C.
         */
        c_trampoline(T& self, this_callback_void method)
            : self { &self }, method { method }
        {
            // and that's all
        }

        // Copy and assignment must correctly change self to point to the correct object,
        // which the containing object must handle.
        c_trampoline(const c_trampoline&) = delete;
        c_trampoline& operator=(const c_trampoline&) = delete;

        /**
         * @brief Returns a function pointer that can be passed to whatever wants a legit callback.
         */
        operator c_callback_void() const
        {
            return reinterpret_cast<c_callback_void>((uint8_t*)asm_code + 1); // Plus one to stay in Thumb node.
        }
        /**
         * @brief Returns a function pointer that can be passed to whatever wants a legit callback.
         */
        c_callback_void get_callback()
        {
            return reinterpret_cast<c_callback_void>((uint8_t*)asm_code + 1);
        }
        
        /**
         * @brief Changes the method this trampoline will access.
         * 
         * This saves 16 bytes of RAM over making an entirely new trampoline.
         */
        void set_method(this_callback_void new_method)
        {
            method = new_method;
        }
        /**
         * @brief Returns the currently active pointer-to-member this trampoline adapts.
         */
        this_callback_void get_method()
        {
            return method;
        }
    
    private:
        /**
         * @brief This is the raw assembly code that performs the trampoline action.
         */
        uint16_t __attribute__((aligned(4))) asm_code[4] =
        {
            0x4801, // ldr r0, [pc, #4] ; self
            0x4902, // ldr r1, [pc, #8] ; method
            0x4708, // bx r1
            0xBF00, // nop ; required for data/instruction alignment
        };

        /**
         * @brief This is just a reference back to the object this is trying to wrap.
         */
        T* const self; // DO NOT change the order of this member or asm_code will be invalid!
        /**
         * @brief This is the actual member function pointer being wrapped.
         */
        this_callback_void method; // DO NOT change the order of this member or asm_code will be invalid!
};



#endif /* C_TRAMPOLINE_H */
// ARM GCC 11.2.1 -mcpu=cortex-m0plus -mthumb -Wl,--build-id
