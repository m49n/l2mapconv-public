#pragma once

#include "Archive.h"
#include "Index.h"
#include "Name.h"
#include "ObjectRef.h"
#include "Property.h"

#include <cstdint>
#include <ostream>
#include <string>

namespace unreal {

struct StateFrame {
  Index node;
  Index state_node;
  std::uint64_t probe_mask;
  std::int32_t latent_action;
  Index offset;

  friend auto operator>>(Archive &archive, StateFrame &state_frame)
      -> Archive &;
};

enum ObjectFlags {
  RF_Transactional = 0x00000001, // Object is transactional.
  RF_Unreachable = 0x00000002,   // Object is not reachable on the object graph.
  RF_Public = 0x00000004,        // Object is visible outside its package.
  RF_TagImp = 0x00000008,        // Temporary import tag in load/save.
  RF_TagExp = 0x00000010,        // Temporary export tag in load/save.
  RF_SourceModified = 0x00000020, // Modified relative to source files.
  RF_TagGarbage = 0x00000040,     // Check during garbage collection.
  RF_Final = 0x00000080,          // Object is not visible outside of class.
  RF_PerObjectLocalized =
      0x00000100, // Object is localized by instance name, not by class.
  RF_NeedLoad = 0x00000200, // During load, indicates object needs loading.
  RF_HighlightedName =
      0x00000400, // A hardcoded name which should be syntax-highlighted.
  RF_EliminateObject =
      0x00000400, // NULL out references to this during garbage collecion.
  RF_InSingularFunc = 0x00000800, // In a singular function.
  RF_RemappedName = 0x00000800,   // Name is remapped.
  RF_Suppress =
      0x00001000, // warning: Mirrored in UnName.h. Suppressed log name.
  RF_StateChanged = 0x00001000,  // Object did a state change.
  RF_InEndState = 0x00002000,    // Within an EndState call.
  RF_Transient = 0x00004000,     // Don't save object.
  RF_Preloading = 0x00008000,    // Data is being preloaded from file.
  RF_LoadForClient = 0x00010000, // In-file load for client.
  RF_LoadForServer = 0x00020000, // In-file load for client.
  RF_LoadForEdit = 0x00040000,   // In-file load for client.
  RF_Standalone =
      0x00080000, // Keep object around for editing even if unreferenced.
  RF_NotForClient = 0x00100000,   // Don't load this object for the game client.
  RF_NotForServer = 0x00200000,   // Don't load this object for the game server.
  RF_NotForEdit = 0x00400000,     // Don't load this object for the editor.
  RF_Destroyed = 0x00800000,      // Object Destroy has already been called.
  RF_NeedPostLoad = 0x01000000,   // Object needs to be postloaded.
  RF_HasStack = 0x02000000,       // Has execution stack.
  RF_Native = 0x04000000,         // Native (UClass only).
  RF_Marked = 0x08000000,         // Marked (for debugging).
  RF_ErrorShutdown = 0x10000000,  // ShutdownAfterError called.
  RF_DebugPostLoad = 0x20000000,  // For debugging Serialize calls.
  RF_DebugSerialize = 0x40000000, // For debugging Serialize calls.
  RF_DebugDestroy = 0x80000000,   // For debugging Destroy calls.
  RF_ContextFlags =
      RF_NotForClient | RF_NotForServer | RF_NotForEdit, // All context flags.
  RF_LoadContextFlags = RF_LoadForClient | RF_LoadForServer |
                        RF_LoadForEdit, // Flags affecting loading.
  RF_Load = RF_ContextFlags | RF_LoadContextFlags | RF_Public | RF_Final |
            RF_Standalone | RF_Native | RF_SourceModified | RF_Transactional |
            RF_HasStack |
            RF_PerObjectLocalized, // Flags to load from Unrealfiles.
  RF_Keep = RF_Native | RF_Marked |
            RF_PerObjectLocalized, // Flags to persist across loads.
  RF_ScriptMask = RF_Transactional | RF_Public | RF_Final | RF_Transient |
                  RF_NotForClient | RF_NotForServer | RF_NotForEdit |
                  RF_Standalone // Script-accessible flags.
};

class Object {
public:
  Name name;

  std::uint32_t flags;
  StateFrame state_frame; // Main script execution stack.

  explicit Object(Archive &archive) : archive{archive} {}
  virtual ~Object() {}

  virtual void deserialize();

  // Returns true if property consumed.
  virtual auto set_property(const Property &) -> bool { return false; }

  auto full_name() const -> std::string;

  friend auto operator<<(std::ostream &output, const Object &object)
      -> std::ostream &;

protected:
  Archive &archive;
};

} // namespace unreal
