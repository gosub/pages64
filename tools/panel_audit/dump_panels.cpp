// dump_panels - print every module's real panel geometry as JSON, for
// panel_audit.py.
//
// Links the plugin's own build objects, runs its init() and builds each
// ModuleWidget with no module (as the module browser does), so positions come
// from the actual widget code, loops and arithmetic included. For each model:
// the panel size, every child widget's box (kind: input, output, param,
// light, screw, display, other) and every shape of the panel SVG with its
// fill color and bounds. All lengths in mm.
//
//   dump_panels <repo root> <Rack system dir> [slug ...]
#include <rack.hpp>
#include <cstdio>
#include <set>

using namespace rack;

extern void init(rack::plugin::Plugin* p);

static const float PX_PER_MM = RACK_GRID_WIDTH / 5.08f;   // 75 dpi

static void printBox(const char* kind, math::Rect r, bool& first) {
    printf("%s\n      {\"kind\": \"%s\", \"x\": %.3f, \"y\": %.3f, \"w\": %.3f, \"h\": %.3f}",
           first ? "" : ",", kind,
           r.pos.x / PX_PER_MM, r.pos.y / PX_PER_MM,
           r.size.x / PX_PER_MM, r.size.y / PX_PER_MM);
    first = false;
}

static const char* kindOf(widget::Widget* w) {
    if (auto* p = dynamic_cast<app::PortWidget*>(w))
        return p->type == engine::Port::INPUT ? "input" : "output";
    if (dynamic_cast<app::ParamWidget*>(w))  return "param";
    if (dynamic_cast<app::LightWidget*>(w))  return "light";
    if (dynamic_cast<app::SvgScrew*>(w))     return "screw";
    if (dynamic_cast<app::LedDisplay*>(w))   return "display";
    return "other";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: dump_panels <repo root> <Rack system dir>\n");
        return 2;
    }
    asset::systemDir = argv[2];
    contextSet(new Context);   // widgets like MidiDisplay reach for APP

    plugin::Plugin* p = new plugin::Plugin;
    p->path = argv[1];
    init(p);

    printf("[");
    bool firstModel = true;
    std::set<std::string> only(argv + 3, argv + argc);
    for (plugin::Model* model : p->models) {
        if (!only.empty() && !only.count(model->slug)) continue;
        app::ModuleWidget* mw = model->createModuleWidget(nullptr);
        printf("%s\n  {\"slug\": \"%s\", \"w\": %.3f, \"h\": %.3f,\n    \"widgets\": [",
               firstModel ? "" : ",", model->slug.c_str(),
               mw->box.size.x / PX_PER_MM, mw->box.size.y / PX_PER_MM);
        firstModel = false;

        bool first = true;
        widget::Widget* panel = mw->getPanel();
        for (widget::Widget* child : mw->children) {
            if (child == panel) continue;
            printBox(kindOf(child), child->box, first);
        }
        printf("],\n    \"shapes\": [");

        first = true;
        auto* svgPanel = dynamic_cast<app::SvgPanel*>(panel);
        if (svgPanel && svgPanel->svg && svgPanel->svg->handle) {
            for (NSVGshape* s = svgPanel->svg->handle->shapes; s; s = s->next) {
                if (!(s->flags & NSVG_FLAGS_VISIBLE)) continue;
                unsigned fill = s->fill.type == NSVG_PAINT_COLOR ? s->fill.color : 0;
                unsigned stroke = s->stroke.type == NSVG_PAINT_COLOR ? s->stroke.color : 0;
                unsigned c = fill ? fill : stroke;   // ABGR
                printf("%s\n      {\"color\": \"#%02x%02x%02x\", \"fill\": %s, "
                       "\"x0\": %.3f, \"y0\": %.3f, \"x1\": %.3f, \"y1\": %.3f}",
                       first ? "" : ",", c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff,
                       fill ? "true" : "false",
                       s->bounds[0] / PX_PER_MM, s->bounds[1] / PX_PER_MM,
                       s->bounds[2] / PX_PER_MM, s->bounds[3] / PX_PER_MM);
                first = false;
            }
        }
        printf("]}");
        // Not deleted: ModuleWidget's destructor expects a live Rack scene,
        // and this process exits right after the dump anyway.
    }
    printf("\n]\n");
    return 0;
}
