// rack_host - the bits of a Rack host a test needs: an app context with an
// engine at a given sample rate (kits read APP->engine->getSampleRate()).
//
// The SDK marks these internal for plugins, through rack.hpp. A test is a
// host, not a plugin, so this file includes the individual headers instead,
// where the guard is off. Kept separate so no test source needs to.
#include <context.hpp>
#include <engine/Engine.hpp>

void rackHostInit(float sampleRate) {
    rack::contextSet(new rack::Context);
    APP->engine = new rack::engine::Engine;
    APP->engine->setSampleRate(sampleRate);
}
