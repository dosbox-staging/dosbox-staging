
#ifndef RIFFCPP_EXPORT_H
#define RIFFCPP_EXPORT_H

#ifdef RIFFCPP_STATIC_DEFINE
#  define RIFFCPP_EXPORT
#  define RIFFCPP_NO_EXPORT
#else
#  ifndef RIFFCPP_EXPORT
#    ifdef riffcpp_EXPORTS
        /* We are building this library */
#      define RIFFCPP_EXPORT 
#    else
        /* We are using this library */
#      define RIFFCPP_EXPORT 
#    endif
#  endif

#  ifndef RIFFCPP_NO_EXPORT
#    define RIFFCPP_NO_EXPORT 
#  endif
#endif

#ifndef RIFFCPP_DEPRECATED
#  define RIFFCPP_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef RIFFCPP_DEPRECATED_EXPORT
#  define RIFFCPP_DEPRECATED_EXPORT RIFFCPP_EXPORT RIFFCPP_DEPRECATED
#endif

#ifndef RIFFCPP_DEPRECATED_NO_EXPORT
#  define RIFFCPP_DEPRECATED_NO_EXPORT RIFFCPP_NO_EXPORT RIFFCPP_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef RIFFCPP_NO_DEPRECATED
#    define RIFFCPP_NO_DEPRECATED
#  endif
#endif

#endif /* RIFFCPP_EXPORT_H */
