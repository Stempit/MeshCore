#pragma once

#ifdef WITH_KVX_DEFAULTS

#include <helpers/IdentityStore.h>   // FILESYSTEM
#include <string.h>

// First-boot seeding of the KVX (Kirov) regional defaults, replayed through
// the normal CLI dispatch so every save/migration path applies. Works with
// any mesh object exposing handleCommand (MyMesh, ObserverMesh).
//
// Applies ONLY to a never-configured device (no /prefs.json, no /com_prefs,
// no /regions2) — saved user settings are never overwritten. Ends with a
// reboot when radio params were seeded ("reboot to apply"); the reboot
// happens before main.cpp queues the boot advert, so nothing is ever
// transmitted on the default (pre-KVX) frequency.
namespace kvx {

constexpr size_t kMaxCmdLen = 64;  // longest seed command is well under this
constexpr size_t kReplyLen = 160;  // replies are short "OK ..." strings

constexpr const char* kPrefsSeed[] = {
    "set radio 868.731,62.5,8,8",   // freq/bw/sf/cr; COMMA-separated (parseTextParts default separator is comma); needs reboot to apply
    "set path.hash.mode 1",         // 2-byte path hashes on air (TX uses mode+1)
    "set cad on",
    "set txdelay 2",
    "set rxdelay 5",
    "set loop.detect minimal",
    "set tx 22",
    "set advert.interval 240",
    "set flood.advert.interval 24",
    "set name KRV_repeater",
#ifdef WITH_MQTT_UPLINK
    "set mqtt.iata KVX",
#endif
};

constexpr const char* kRegionsSeed[] = {
    "region put ru",       // parent must exist before kvx
    "region put kvx ru",
    "region allowf *",
    "region save",
};

template <typename MeshT>
void replay(MeshT& mesh, const char* const commands[], size_t count) {
  char cmd[kMaxCmdLen];
  char reply[kReplyLen];
  for (size_t i = 0; i < count; ++i) {
    strncpy(cmd, commands[i], sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = 0;
    reply[0] = 0;
    mesh.handleCommand(0, cmd, reply);
    Serial.printf("[kvx] %s -> %s\n", commands[i], reply[0] ? reply : "(no reply)");
  }
}

template <typename MeshT>
void applyIfFresh(FILESYSTEM* fs, MeshT& mesh) {
  // A device that has saved any settings is considered configured — never
  // touch it. A device that only ever booted on factory defaults (no prefs
  // files, no regions file) gets the KVX defaults.
  bool prefs_fresh = !fs->exists("/prefs.json") && !fs->exists("/com_prefs");
  bool regions_fresh = !fs->exists("/regions2");
  Serial.printf("[kvx] seed check: prefs %s, regions %s\n", prefs_fresh ? "FRESH (will seed)" : "kept (existing config)",
                regions_fresh ? "FRESH (will seed)" : "kept (existing config)");

  if (prefs_fresh) {
    replay(mesh, kPrefsSeed, sizeof(kPrefsSeed) / sizeof(kPrefsSeed[0]));
  }
  if (regions_fresh) {
    replay(mesh, kRegionsSeed, sizeof(kRegionsSeed) / sizeof(kRegionsSeed[0]));
  }
  if (prefs_fresh) {
    // Radio params need a reboot to apply; this call never returns.
    char cmd[] = "reboot";
    char reply[kReplyLen];
    mesh.handleCommand(0, cmd, reply);
  }
}

}

#endif
