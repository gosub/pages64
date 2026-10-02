// persistence - saved-data checks for one module, compiled once per module:
//   -DMODULE_SRC='"../src/Buttons64.cpp"' -DMODULE_T=Buttons64
//
// round_trip   dataToJson → dataFromJson into a fresh module → dataToJson
//              gives identical JSON. Patch save/load and the button-6 temp
//              snapshot both go through this path.
// format_key   the saved data carries "v" = P64::DATA_FORMAT.
// legacy_color the saved data with every *Color field re-encoded as a raw
//              MkII velocity and "v" removed (the pre-2.23.0 format) loads
//              back to the same colors.
//
// Prints one CSV row per check: module,check,pass. Exits nonzero on failure.
#include MODULE_SRC

#include <cstdio>
#include <cstring>

#define STR2(x) #x
#define STR(x) STR2(x)

static int failures = 0;

static void report(const char* check, bool pass) {
    printf("%s,%s,%s\n", STR(MODULE_T), check, pass ? "PASS" : "FAIL");
    if (!pass) failures++;
}

static bool isColorKey(const char* key) {
    return std::strstr(key, "Color") != nullptr;
}

static json_t* legacyEncode(json_t* v) {
    if (json_is_integer(v))
        return json_integer(P64::mk2Velocity((uint8_t) json_integer_value(v)));
    if (json_is_array(v)) {
        json_t* out = json_array();
        size_t i; json_t* e;
        json_array_foreach(v, i, e)
            json_array_append_new(out, legacyEncode(e));
        return out;
    }
    return json_incref(v);
}

static json_t* roundTrip(json_t* data) {
    MODULE_T fresh;
    fresh.dataFromJson(data);
    return fresh.dataToJson();
}

int main(int argc, char** argv) {
    bool header = !(argc > 1 && !std::strcmp(argv[1], "--no-header"));
    if (header) printf("module,check,pass\n");
    rack::random::init();

    MODULE_T m;
    json_t* saved = m.dataToJson();
    if (!saved) {   // a module with no saved data has nothing to check
        report("no_data", true);
        return 0;
    }

    json_t* again = roundTrip(saved);
    report("round_trip", json_equal(saved, again));
    json_decref(again);

    json_t* v = json_object_get(saved, "v");
    report("format_key", v && json_integer_value(v) == P64::DATA_FORMAT);

    json_t* legacy = json_object();
    const char* key; json_t* val;
    bool anyColor = false;
    json_object_foreach(saved, key, val) {
        if (!std::strcmp(key, "v")) continue;
        if (isColorKey(key)) {
            anyColor = true;
            json_object_set_new(legacy, key, legacyEncode(val));
        } else {
            json_object_set(legacy, key, val);
        }
    }
    if (anyColor) {
        json_t* migrated = roundTrip(legacy);
        report("legacy_color", json_equal(saved, migrated));
        json_decref(migrated);
    }

    json_decref(legacy);
    json_decref(saved);
    return failures ? 1 : 0;
}
