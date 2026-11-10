#pragma once

// Mod release version — bump on every mod release. The MQTT client identity
// ("observer_meshcoretel_<ver>") and the status firmware string
// ("v1.18.0" + segment + version) are derived from this.
#define MESHCORETEL_MOD_VERSION "v0.1.2"

// Firmware-string segment; the kvx flavour is selected here at compile time.
#ifdef WITH_KVX_DEFAULTS
  #define MESHCORETEL_MOD_SEGMENT "_meshcoretel_kvx_mod_"
#else
  #define MESHCORETEL_MOD_SEGMENT "_meshcoretel_mod_"
#endif
