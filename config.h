// Configuration filename (stored in OS user config directory). On Linux usualy ~/.config
#define CONFIG "acom2000.conf"

// No need to change these
#define COMPUTER  0x71    // computer address (q in ASCII)
#define AMPLIFIER 0x41    // amplifier address (A in ASCII)
#define BROADCAST 0xff    // broadcast address

// Icon directory (Linux fallback; Windows/macOS typically bundle icons alongside the binary)
#ifndef _WIN32
#define ICON_DIR "/usr/local/share/icons/hicolor/32x32/apps"
#endif

// Update interval in milliseconds
#define UPDATE 50

// SWR warning threshold
#define SWR_WARN 2.0
