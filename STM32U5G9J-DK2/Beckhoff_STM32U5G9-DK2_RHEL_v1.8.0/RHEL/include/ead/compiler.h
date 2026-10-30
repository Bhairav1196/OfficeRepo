/**
 * @file compiler.h
 * @brief Compiler portability, attributes and warning-control macros.
 * @author Prathamesh Chipkar
 */
#ifndef EAD_COMPILER_H
#define EAD_COMPILER_H

#if defined(__GNUC__) || defined(__clang__)
#define EAD_PRINTF_FORMAT(format_index, first_argument) \
    __attribute__((format(printf, format_index, first_argument)))
#else
#define EAD_PRINTF_FORMAT(format_index, first_argument)
#endif

#endif
