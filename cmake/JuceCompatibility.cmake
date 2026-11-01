# Narrow compatibility fixes for the pinned JUCE 6 fork. No change to the audio protocol.
function(aw_patch_juce relative before after)
    set(path "${JUCE_SOURCE_DIR}/${relative}")
    file(READ "${path}" source)
    string(REPLACE "\r\n" "\n" source "${source}")
    string(FIND "${source}" "${after}" already_patched)
    if(NOT already_patched EQUAL -1)
        return()
    endif()
    string(FIND "${source}" "${before}" original)
    if(original EQUAL -1)
        message(FATAL_ERROR "Pinned JUCE compatibility context changed: ${relative}")
    endif()
    string(REPLACE "${before}" "${after}" source "${source}")
    file(WRITE "${path}" "${source}")
endfunction()

if(WIN32)
    aw_patch_juce(modules/juce_core/system/juce_TargetPlatform.h
[=[  #define JUCE_LITTLE_ENDIAN 1

  #define JUCE_INTEL 1
#endif]=]
[=[  #define JUCE_LITTLE_ENDIAN 1

  // AudioWire: the pinned fork predates native Windows ARM64 detection.
  #if defined (_M_ARM64) || defined (_M_ARM) || defined (__aarch64__)
    #define JUCE_ARM 1
  #else
    #define JUCE_INTEL 1
  #endif
#endif]=])
    aw_patch_juce(modules/juce_core/native/juce_win32_SystemStats.cpp
        "#if JUCE_MSVC && ! defined (__INTEL_COMPILER)"
        "#if JUCE_INTEL && JUCE_MSVC && ! defined (__INTEL_COMPILER)")
    aw_patch_juce(modules/juce_core/native/juce_win32_SystemStats.cpp
        "#if JUCE_MINGW || JUCE_CLANG\nstatic void callCPUID"
[=[#if JUCE_ARM
static void callCPUID (int result[4], int)
{
    // There is no x86 CPUID on ARM; keep all x86 capability bits clear.
    for (int i = 0; i < 4; ++i) result[i] = 0;
}
#elif JUCE_MINGW || JUCE_CLANG
static void callCPUID]=])
    aw_patch_juce(modules/juce_core/native/juce_win32_SystemStats.cpp
        "   #if JUCE_MSVC\n    // MS intrinsics version..."
        "   #if JUCE_ARM\n    return 0; // CPU cycle probing is unavailable in this pinned ARM port.\n   #elif JUCE_MSVC\n    // MS intrinsics version...")
    aw_patch_juce(modules/juce_core/native/juce_win32_SystemStats.cpp
        "int SystemStats::getCpuSpeedInMegahertz()\n{\n    auto cycles"
        "int SystemStats::getCpuSpeedInMegahertz()\n{\n   #if JUCE_ARM\n    return 0; // Report unknown instead of a fabricated x86 clock result.\n   #else\n    auto cycles")
    aw_patch_juce(modules/juce_core/native/juce_win32_SystemStats.cpp
        "    }\n}\n\n\n//==============================================================================\nbool Time::setSystemTimeToThisTime() const"
        "    }\n   #endif\n}\n\n\n//==============================================================================\nbool Time::setSystemTimeToThisTime() const")
endif()

if(CMAKE_HOST_APPLE)
    # juceaide is a headless host build tool; AudioWire ships no macOS GUI app.
    # Its unused window-snapshot helper calls an API removed from the macOS 15 SDK.
    aw_patch_juce(modules/juce_gui_basics/native/juce_mac_Windowing.mm
        "static Image createNSWindowSnapshot (NSWindow* nsWindow)\n{\n    JUCE_AUTORELEASEPOOL"
        "static Image createNSWindowSnapshot (NSWindow* nsWindow)\n{\n   #if MAC_OS_X_VERSION_MAX_ALLOWED >= 150000\n    ignoreUnused (nsWindow);\n    return {}; // AudioWire: unused by the headless juceaide tool.\n   #else\n    JUCE_AUTORELEASEPOOL")
    aw_patch_juce(modules/juce_gui_basics/native/juce_mac_Windowing.mm
        "        return result;\n    }\n}\n\nImage createSnapshotOfNativeWindow (void* nativeWindowHandle)"
        "        return result;\n    }\n   #endif\n}\n\nImage createSnapshotOfNativeWindow (void* nativeWindowHandle)")
endif()
