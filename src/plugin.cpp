#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
    pluginInstance = p;
    p->addModel(modelBase);
    p->addModel(modelButtons64);
    p->addModel(modelGrid64);
    p->addModel(modelSliders64);
}
