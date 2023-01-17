#ifndef PICO_TRAMPOLINE_H
#define PICO_TRAMPOLINE_H

#ifndef __cpp_concepts
#error "Support for C++ concepts is required.  Please add set(CMAKE_CXX_STANDARD 20) to your CMakeLists.txt."
#endif /* __cpp_concepts */

#include "c_trampoline.hpp"

/**
 * @brief irq_handler_t
 */
template<typename T> using irq_trampoline = c_trampoline<T, void>;
/**
 * @brief exception_handler_t
 */
template<typename T> using exception_trampoline = c_trampoline<T, void>;
/**
 * @brief resus_callback_t
 */
template<typename T> using resus_trampoline = c_trampoline<T, void>;
/**
 * @brief rtc_callback_t
 */
template<typename T> using rtc_trampoline = c_trampoline<T, void>;
/**
 * @brief gpio_irq_callback_t
 */
template<typename T> using gpio_irq_trampoline = c_trampoline<T, void, uint32_t, uint32_t>;
/**
 * @brief hardware_alarm_callback_t
 */
template<typename T> using hardware_alarm_trampoline = c_trampoline<T, void, uint32_t>;
/**
 * @brief repeating_timer_callback_t
 */
template<typename T> using repeating_timer_trampoline = c_trampoline<T, bool, struct repeating_timer*>;
/**
 * @brief alarm_callback_t
 */
template<typename T> using alarm_trampoline = c_trampoline<T, int64_t, int32_t, void*>;

/**
 * @brief Helper macro to make it easier to add a trampoline to a class.
 * 
 * @param TYPE One of { irq, exception, resus, rtc, gpio_irq, hardware_alarm, repeating_timer, alarm }
 * @param CLASS Type of the current class, which is surprisingly hard to get.
 * @param METHOD Name of method that does the real work.
 * @param HANDLER Name of handler variable that will be passed to the C API routines.
 */
#define MAKE_TRAMPOLINE(TYPE, CLASS, METHOD, HANDLER) TYPE ## _trampoline<CLASS> HANDLER { *this, &CLASS::METHOD }


#endif /* PICO_TRAMPOLINE_H */
