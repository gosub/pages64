#include "plugin.hpp"

Plugin* pluginInstance;

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
}
