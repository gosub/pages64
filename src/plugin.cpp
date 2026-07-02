#include "plugin.hpp"

Plugin* pluginInstance;

namespace P64 {
SharedKey sharedKey;
}

void init(Plugin* p) {
    pluginInstance = p;
    p->addModel(modelBase);
    p->addModel(modelButtons64);
    p->addModel(modelGrid64);
    p->addModel(modelSliders64);
    p->addModel(modelFlin64);
    p->addModel(modelStep64);
    p->addModel(modelCafe64);
    p->addModel(modelGome64);
    p->addModel(modelNotes64);
    p->addModel(modelEuclid64);
    p->addModel(modelBounce64);
    p->addModel(modelMlr64);
    p->addModel(modelNotes8);
    p->addModel(modelLife64);
    p->addModel(modelSequencer64);
    p->addModel(modelInertia64);
    p->addModel(modelKeys64);
    p->addModel(modelMeadow64);
    p->addModel(modelPads64);
    p->addModel(modelXY64);
}
