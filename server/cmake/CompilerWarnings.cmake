# Maximum warning level for our own targets. Third-party headers come from imported
# targets, which CMake marks as SYSTEM includes: their warnings are not reported.
function(uno_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /utf-8
            /external:W0
            $<$<BOOL:${UNO_WARNINGS_AS_ERRORS}>:/WX>)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
            -Wnull-dereference
            $<$<BOOL:${UNO_WARNINGS_AS_ERRORS}>:-Werror>)
    endif()
endfunction()
