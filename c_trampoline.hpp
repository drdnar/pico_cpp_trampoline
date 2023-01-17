#ifndef C_TRAMPOLINE_H
#define C_TRAMPOLINE_H
#include <stdint.h>

#ifndef __cpp_concepts
#error "Support for C++ concepts is required."
#endif /* __cpp_concepts */

/**
 * @brief Check if a type can be passed in a single Thumb register. 
 */
template<typename T> concept FitsInRegister = sizeof(T) <= 4;
/**
 * @brief Check if a type is actually just void.
 */
template<typename T> concept VoidReturn = requires (T (*x)()) { x(); };
/**
 * @brief Check if the number of arguments passed is less-than-or-equal-to some value.
 */
template<int Count, typename ... Args> concept NotTooManyArgs = sizeof...(Args) <= Count; 

/**
 * @brief Adapts a non-static class method for use with a C callback.
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
 * @tparam R Return type of function pointer type being adapted.
 * @tparam Args List of zero to three additional parameters to the function.
 */
template<typename T, typename R, FitsInRegister... Args>
requires ((sizeof(R) <= 8) || VoidReturn<R>) && NotTooManyArgs<3, Args...>
struct __attribute__((__packed__)) c_trampoline
{
        /**
         * @brief Type of a member function pointer.
         * 
         * Required for syntax reasons.
         */
        typedef R (T::*member_function_pointer)(Args...);
        /**
         * @brief Type of a function pointer.
         * 
         * Also used for syntax reasons.
         */
        typedef R (*function_pointer)(Args...);

        /**
         * @brief Only reasonable constructor.
         * 
         * @param self Object to bind this wrapper to.
         * @param method Member function pointer you want adapted for use with C.
         */
        c_trampoline(T& self, member_function_pointer method)
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
        operator function_pointer() const
        {
            return reinterpret_cast<function_pointer>((uint8_t*)asm_code + 1); // Plus one to stay in Thumb node.
        }
        /**
         * @brief Returns a function pointer that can be passed to whatever wants a legit callback.
         */
        function_pointer get_callback() const
        {
            return reinterpret_cast<function_pointer>((uint8_t*)asm_code + 1);
        }
        
        /**
         * @brief Changes the method this trampoline will access.
         * 
         * This saves 16 bytes of RAM over making an entirely new trampoline.
         */
    	void set_method(member_function_pointer new_method)
        {
            method = new_method;
        }
        /**
         * @brief Returns the currently active pointer-to-member this trampoline adapts.
         */
        member_function_pointer get_method() const
        {
            return method;
        }

    private:
        /**
         * @brief Conditionally adjusts the size of the asm_code block depending on the number of parameters.
         * 
         * @tparam Count Number of arguments
         */
        template<int Count> struct AsmCode;
        template<int Count> requires (Count == 0) struct AsmCode<Count>
        {
            uint8_t volatile __attribute__((aligned(4))) code[8] =
            {
                            // ; PC is measured from the address of the start of the next instruction pair.
                0x01, 0x48, // ldr r0, [pc, #4]     ; self
                0x01, 0x4B, // ldr r3, [pc, #8]     ; method (note that both LDRs are in the SAME pair)
                0x18, 0x47, // bx  r3
                0x00, 0xBF, // nop
            };
        };
        template<int Count> requires (Count == 1) struct AsmCode<Count>
        {
            uint8_t volatile __attribute__((aligned(4))) code[8] =
            {
                0x01, 0x46, // mov r1, r0
                0x01, 0x48, // ldr r0, [pc, #4]     ; self
                0x01, 0x4B, // ldr r3, [pc, #4]     ; method (note that both LDRs are in DIFFERENT pairs)
                0x18, 0x47, // bx  r3
            };
        };
        template<int Count> requires (Count == 2) struct AsmCode<Count>
        {
            uint8_t volatile __attribute__((aligned(4))) code[12] =
            {
                0x0A, 0x46, // mov r2, r1
                0x01, 0x46, // mov r1, r0
                0x01, 0x48, // ldr r0, [pc, #4]     ; self
                0x02, 0x4B, // ldr r3, [pc, #8]     ; method (note that both LDRs are in the SAME pair)
                0x18, 0x47, // bx  r3
                0x00, 0xBF, // nop
            };
        };
        template<int Count> requires (Count == 3) struct AsmCode<Count>
        {
            uint8_t volatile __attribute__((aligned(4))) code[16] =
            {
                0x13, 0x46, // mov r3, r2
                0x0a, 0x46, // mov r2, r1
                0x01, 0x46, // mov r1, r0
                            // ; r12 also need not be preserved
                0x01, 0x48, // ldr r0, [pc, #12]    ; method
                0x84, 0x46, // mov r12, r0
                0x01, 0x48, // ldr r0, [pc, #4]     ; self
                0x60, 0x47, // bx r12
                0x00, 0xbf, // nop
            };
        };
        AsmCode<sizeof...(Args)> asm_code;
        /* Alternatively, we could derive the this pointer from PC, 
         * but that requires introducing the offset as a parameter to the template.
         * 
         * 0x00, 0xBF, // nop ; For padding/instruction alignment
         * 0x78, 0x46, // mov r0, pc
         * offset_byte, 0x38, // subs r0, offset */  

        /**
         * @brief This is just a reference back to the object this is trying to wrap.
         */
        T* const self; // DO NOT change the order of this member or asm_code will be invalid!
        /**
         * @brief This is the actual member function pointer being wrapped.
         */
        member_function_pointer method; // DO NOT change the order of this member or asm_code will be invalid!
};

#endif /* C_TRAMPOLINE_H */
