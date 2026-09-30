# Sanitizers are applied globally (every target, tests included) so that all our code
# is instrumented consistently. See docs/adr/0004-sanitizers-per-platform.md.
if(NOT UNO_SANITIZERS)
    return()
endif()

if(MSVC)
    # MSVC only provides AddressSanitizer (no UBSan).
    add_compile_options(/fsanitize=address /Zi)
    add_link_options(/INCREMENTAL:NO)
    # /RTC runtime checks are incompatible with /fsanitize=address.
    set(CMAKE_MSVC_RUNTIME_CHECKS "")
    # vcpkg libraries are not instrumented: disable every STL annotation (vector, string,
    # optional...) that would otherwise produce LNK2038 mismatch errors at link time.
    add_compile_definitions(_DISABLE_STL_ANNOTATION)
else()
    set(UNO_SANITIZER_FLAGS
        -fsanitize=address,undefined
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer)
    add_compile_options(${UNO_SANITIZER_FLAGS})
    add_link_options(${UNO_SANITIZER_FLAGS})
endif()
