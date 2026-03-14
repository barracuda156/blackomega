# Last.fm library configuration

if (OMEGA_WIN32)
    if (OMEGA_MSVC16)
        set(LASTFM_INCLUDE_DIR "${BLACKOMEGA_UTILS}/lastfm/include")
        set(LASTFM_LIBRARY_DIR "${BLACKOMEGA_UTILS}/lastfm/lib")

        if (TIGER_DEBUG_BUILD)
            set(LASTFM_LIBRARY_NAME "lastfmd")
        else (TIGER_DEBUG_BUILD)
            set(LASTFM_LIBRARY_NAME "lastfm")
        endif (TIGER_DEBUG_BUILD)

        include_directories(AFTER "${LASTFM_INCLUDE_DIR}")
        link_directories("${LASTFM_LIBRARY_DIR}")
    endif (OMEGA_MSVC16)

elseif (OMEGA_MACOSX)
    if (${TIGER_SYSTEM_DEPS})
        set(LASTFM_INCLUDE_DIR "${BLACKOMEGA_PREFIX}/include")
        set(LASTFM_LIBRARY_DIR "${BLACKOMEGA_PREFIX}/lib")
    else (${TIGER_SYSTEM_DEPS})
        set(LASTFM_INCLUDE_DIR "${BLACKOMEGA_UTILS}/lastfm/include")
        set(LASTFM_LIBRARY_DIR "${BLACKOMEGA_UTILS}/lastfm/lib")
    endif (${TIGER_SYSTEM_DEPS})

    set(LASTFM_LIBRARY_NAME "lastfm")

    include_directories(AFTER "${LASTFM_INCLUDE_DIR}")
    link_directories("${LASTFM_LIBRARY_DIR}")

elseif (OMEGA_LINUX)
    if (${TIGER_SYSTEM_DEPS})
        set(LASTFM_INCLUDE_DIR "${BLACKOMEGA_PREFIX}/include")
        set(LASTFM_LIBRARY_DIR "${BLACKOMEGA_PREFIX}/lib")
    else (${TIGER_SYSTEM_DEPS})
        set(LASTFM_INCLUDE_DIR "${BLACKOMEGA_UTILS}/lastfm/include")
        set(LASTFM_LIBRARY_DIR "${BLACKOMEGA_UTILS}/lastfm/lib")
    endif (${TIGER_SYSTEM_DEPS})

    set(LASTFM_LIBRARY_NAME "lastfm")

    include_directories(AFTER "${LASTFM_INCLUDE_DIR}")
    link_directories("${LASTFM_LIBRARY_DIR}")

endif (OMEGA_WIN32)
