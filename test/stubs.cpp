// stubs - the plugin globals a single-module test binary would otherwise
// miss. Every model pointer is weak, so the one module compiled into the
// binary supplies its own definition and the rest stay null.
#include "../src/plugin.hpp"

Plugin* pluginInstance = nullptr;

namespace P64 {
SharedKey sharedKey;
}

#define WEAK_MODEL(name) __attribute__((weak)) Model* name = nullptr;
WEAK_MODEL(modelBase)        WEAK_MODEL(modelButtons64)   WEAK_MODEL(modelGrid64)
WEAK_MODEL(modelSliders64)   WEAK_MODEL(modelFlin64)      WEAK_MODEL(modelStep64)
WEAK_MODEL(modelCafe64)      WEAK_MODEL(modelGome64)      WEAK_MODEL(modelNotes64)
WEAK_MODEL(modelEuclid64)    WEAK_MODEL(modelBounce64)    WEAK_MODEL(modelMlr64)
WEAK_MODEL(modelNotes8)      WEAK_MODEL(modelLife64)      WEAK_MODEL(modelSequencer64)
WEAK_MODEL(modelInertia64)   WEAK_MODEL(modelKeys64)      WEAK_MODEL(modelMeadow64)
WEAK_MODEL(modelPads64)      WEAK_MODEL(modelXY64)        WEAK_MODEL(modelRhythm64)
WEAK_MODEL(modelDrums64)     WEAK_MODEL(modelObjects64)   WEAK_MODEL(modelGrains64)
WEAK_MODEL(modelMicro64)     WEAK_MODEL(modelFlood64)     WEAK_MODEL(modelPoly16)
