#!/usr/bin/env python3
"""
gen_patches.py — Generate the example patches in patches/*.vcv.

A Rack 2 .vcv file is a zstd-compressed tar archive containing patch.json
(and an empty modules/ directory). This script builds the three example
patches from scratch; run it from the repo root after changing anything:

    python3 tools/gen_patches.py

Port/param ids of Fundamental modules follow their public source (v2 branch).
The Base64 page chain requires physical adjacency: Base64 is 14 HP, all page
modules are 10 HP, so consecutive chain positions are x=0, 14, 24, 34, …
"""
import json, os, subprocess, tempfile

RACK_VERSION = "2.6.6"

CABLE_COLORS = ["#f3374b", "#ffb437", "#00b56e", "#3695ef", "#8b4ade"]


class Patch:
    def __init__(self):
        self.modules = []
        self.cables = []
        self._id = 0

    def add(self, plugin, model, pos, params=None, data=None, version=None):
        self._id += 1
        m = {"id": self._id, "plugin": plugin, "model": model, "pos": list(pos)}
        if version:
            m["version"] = version
        m["params"] = [{"id": i, "value": v} for i, v in (params or {}).items()]
        if data is not None:
            m["data"] = data
        self.modules.append(m)
        return self._id

    def wire(self, out_mod, out_id, in_mod, in_id):
        self._id += 1
        self.cables.append({
            "id": self._id,
            "outputModuleId": out_mod, "outputId": out_id,
            "inputModuleId": in_mod, "inputId": in_id,
            "color": CABLE_COLORS[len(self.cables) % len(CABLE_COLORS)],
        })

    def write(self, path):
        patch = {"version": RACK_VERSION, "modules": self.modules, "cables": self.cables}
        with tempfile.TemporaryDirectory() as d:
            os.mkdir(os.path.join(d, "modules"))
            with open(os.path.join(d, "patch.json"), "w") as f:
                json.dump(patch, f, indent=2)
            tar = path + ".tar"
            subprocess.run(["tar", "-cf", os.path.abspath(tar),
                            "./modules", "./patch.json"], cwd=d, check=True)
        subprocess.run(["zstd", "-q", "-f", tar, "-o", path], check=True)
        os.remove(tar)
        print("wrote", path)


# Shorthands for the modules used (plugin, model)
P64  = "pages64"
FUND = "Fundamental"
CORE = "Core"

# Fundamental port ids (from the v2 source)
LFO_SQR = 3
VCO_PITCH_IN, VCO_SIN, VCO_TRI, VCO_SAW = 0, 0, 1, 2
ADSR_GATE_IN, ADSR_RETRIG_IN, ADSR_ENV = 4, 5, 0
VCA_CV_IN, VCA_IN, VCA_OUT = 0, 1, 0
SUM_POLY_IN, SUM_MONO_OUT = 0, 0
VCF_FREQ_IN, VCF_IN, VCF_LPF = 0, 3, 0
NOISE_WHITE, NOISE_RED = 0, 2
DELAY_IN, DELAY_OUT = 4, 0
MIX_IN0, MIX_OUT = 0, 0
AUDIO_L, AUDIO_R = 0, 1

# pages64 port ids
BASE_CLK = 0
POLY9 = 8           # Flin64 / Sliders64 / (Step64 uses 8): poly jack after 8 mono
STEP_T1, STEP_T2 = 0, 1
N64_CELL0, N64_PITCH, N64_GATE, N64_RTRG = 0, 0, 1, 2

# Common param sets
LFO_4HZ   = {2: 2.0}                              # freq = 2^2 Hz
PLUCK     = {0: 0.05, 1: 0.45, 2: 0.3, 3: 0.5}    # ADSR a/d/s/r
SUSTAINED = {0: 0.1, 1: 0.5, 2: 0.6, 3: 0.4}
PAD       = {0: 0.3, 1: 0.5, 2: 0.8, 3: 0.7}
HAT       = {0: 0.0, 1: 0.25, 2: 0.0, 3: 0.25}
THUMP     = {0: 0.0, 1: 0.5, 2: 0.0, 3: 0.5}


def patch_flin_sliders():
    """Flin64 polyrhythm gates play 8 voices whose pitches are set live on
    the Sliders64 page: two pages, one instrument."""
    p = Patch()
    base = p.add(P64, "Base64",    (0, 0))
    flin = p.add(P64, "Flin64",    (14, 0))
    sldr = p.add(P64, "Sliders64", (24, 0))

    lfo  = p.add(FUND, "LFO",   (0, 1),  LFO_4HZ)
    vca_p = p.add(FUND, "VCA-1", (13, 1), {0: 0.25})       # slider 0-10V → 0-2.5V pitch
    vco  = p.add(FUND, "VCO",   (18, 1), {2: -24.0})       # 2 octaves down
    adsr = p.add(FUND, "ADSR",  (30, 1), SUSTAINED)
    vca  = p.add(FUND, "VCA-1", (40, 1))
    s    = p.add(FUND, "Sum",   (45, 1), {0: 0.4})
    aud  = p.add(CORE, "AudioInterface2", (50, 1))

    p.wire(lfo, LFO_SQR, base, BASE_CLK)
    p.wire(sldr, POLY9, vca_p, VCA_IN)
    p.wire(vca_p, VCA_OUT, vco, VCO_PITCH_IN)
    p.wire(flin, POLY9, adsr, ADSR_GATE_IN)
    p.wire(vco, VCO_SAW, vca, VCA_IN)
    p.wire(adsr, ADSR_ENV, vca, VCA_CV_IN)
    p.wire(vca, VCA_OUT, s, SUM_POLY_IN)
    p.wire(s, SUM_MONO_OUT, aud, AUDIO_L)
    p.wire(s, SUM_MONO_OUT, aud, AUDIO_R)
    p.write("patches/01_flin_sliders.vcv")


def patch_gome_64notes():
    """The flagship pairing: Gome64 arpeggios → 64Notes voice allocation →
    one polyphonic saw voice, with a touch of delay."""
    p = Patch()
    base = p.add(P64, "Base64", (0, 0))
    gome = p.add(P64, "Gome64", (14, 0))

    lfo  = p.add(FUND, "LFO",   (0, 1), LFO_4HZ)
    n64  = p.add(P64, "64Notes", (13, 1))
    vco  = p.add(FUND, "VCO",   (25, 1))
    adsr = p.add(FUND, "ADSR",  (37, 1), PLUCK)
    vca  = p.add(FUND, "VCA-1", (47, 1))
    s    = p.add(FUND, "Sum",   (52, 1), {0: 0.5})
    dly  = p.add(FUND, "Delay", (56, 1))
    aud  = p.add(CORE, "AudioInterface2", (70, 1))

    p.wire(lfo, LFO_SQR, base, BASE_CLK)
    for i in range(4):
        p.wire(gome, i, n64, N64_CELL0 + i)
    p.wire(n64, N64_PITCH, vco, VCO_PITCH_IN)
    p.wire(n64, N64_GATE, adsr, ADSR_GATE_IN)
    p.wire(n64, N64_RTRG, adsr, ADSR_RETRIG_IN)
    p.wire(vco, VCO_SAW, vca, VCA_IN)
    p.wire(adsr, ADSR_ENV, vca, VCA_CV_IN)
    p.wire(vca, VCA_OUT, s, SUM_POLY_IN)
    p.wire(s, SUM_MONO_OUT, dly, DELAY_IN)
    p.wire(dly, DELAY_OUT, aud, AUDIO_L)
    p.wire(s, SUM_MONO_OUT, aud, AUDIO_R)
    p.write("patches/02_gome_64notes.vcv")


def patch_four_pages():
    """A four-page instrument: Step64 drums (noise), Buttons64 chord drone
    (gate-follow 64Notes), Sliders64 filter control, Gome64 lead."""
    p = Patch()
    base = p.add(P64, "Base64",    (0, 0))
    bttn = p.add(P64, "Buttons64", (14, 0), {0: 0.0, 1: 0.0, 2: 0.0, 3: 0.0})  # all toggle
    step = p.add(P64, "Step64",    (24, 0), data={
        "steps": [
            [True, False, True, False, True, False, True, True],   # T1: hats
            [True, False, False, False, True, False, False, False],  # T2: thumps
            [False] * 8, [False] * 8, [False] * 8, [False] * 8, [False] * 8,
        ]})
    sldr = p.add(P64, "Sliders64", (34, 0))
    gome = p.add(P64, "Gome64",    (44, 0))

    lfo = p.add(FUND, "LFO", (0, 1), LFO_4HZ)

    # Lead: Gome64 → 64Notes → saw → VCF (cutoff from slider column 1) → VCA
    n64a  = p.add(P64, "64Notes", (12, 1))
    vcoa  = p.add(FUND, "VCO",   (24, 1))
    vcf   = p.add(FUND, "VCF",   (36, 1), {0: 0.35, 2: 0.3, 3: 0.6})
    adsra = p.add(FUND, "ADSR",  (48, 1), PLUCK)
    vcaa  = p.add(FUND, "VCA-1", (58, 1))
    suma  = p.add(FUND, "Sum",   (63, 1), {0: 0.5})

    # Drone: Buttons64 toggles → 64Notes in gate-follow → triangle pad
    n64b  = p.add(P64, "64Notes", (70, 1), data={"lenMode": 0})
    vcob  = p.add(FUND, "VCO",   (82, 1), {2: -12.0})
    adsrb = p.add(FUND, "ADSR",  (94, 1), PAD)
    vcab  = p.add(FUND, "VCA-1", (104, 1))
    sumb  = p.add(FUND, "Sum",   (109, 1), {0: 0.4})

    # Drums: Step64 triggers gate two noise voices
    noise = p.add(FUND, "Noise", (116, 1))
    adsrc = p.add(FUND, "ADSR",  (122, 1), HAT)
    vcac  = p.add(FUND, "VCA-1", (132, 1), {0: 0.5})
    adsrd = p.add(FUND, "ADSR",  (137, 1), THUMP)
    vcad  = p.add(FUND, "VCA-1", (147, 1))

    mix = p.add(FUND, "Mixer", (152, 1), {0: 0.7})
    aud = p.add(CORE, "AudioInterface2", (162, 1))

    p.wire(lfo, LFO_SQR, base, BASE_CLK)
    # lead
    for i in range(4):
        p.wire(gome, i, n64a, N64_CELL0 + i)
    p.wire(n64a, N64_PITCH, vcoa, VCO_PITCH_IN)
    p.wire(n64a, N64_GATE, adsra, ADSR_GATE_IN)
    p.wire(n64a, N64_RTRG, adsra, ADSR_RETRIG_IN)
    p.wire(vcoa, VCO_SAW, vcf, VCF_IN)
    p.wire(sldr, 0, vcf, VCF_FREQ_IN)            # slider column 1 = cutoff
    p.wire(vcf, VCF_LPF, vcaa, VCA_IN)
    p.wire(adsra, ADSR_ENV, vcaa, VCA_CV_IN)
    p.wire(vcaa, VCA_OUT, suma, SUM_POLY_IN)
    p.wire(suma, SUM_MONO_OUT, mix, MIX_IN0 + 0)
    # drone
    for i in range(4):
        p.wire(bttn, i, n64b, N64_CELL0 + i)
    p.wire(n64b, N64_PITCH, vcob, VCO_PITCH_IN)
    p.wire(n64b, N64_GATE, adsrb, ADSR_GATE_IN)
    p.wire(n64b, N64_RTRG, adsrb, ADSR_RETRIG_IN)
    p.wire(vcob, VCO_TRI, vcab, VCA_IN)
    p.wire(adsrb, ADSR_ENV, vcab, VCA_CV_IN)
    p.wire(vcab, VCA_OUT, sumb, SUM_POLY_IN)
    p.wire(sumb, SUM_MONO_OUT, mix, MIX_IN0 + 1)
    # drums
    p.wire(step, STEP_T1, adsrc, ADSR_GATE_IN)
    p.wire(noise, NOISE_WHITE, vcac, VCA_IN)
    p.wire(adsrc, ADSR_ENV, vcac, VCA_CV_IN)
    p.wire(vcac, VCA_OUT, mix, MIX_IN0 + 2)
    p.wire(step, STEP_T2, adsrd, ADSR_GATE_IN)
    p.wire(noise, NOISE_RED, vcad, VCA_IN)
    p.wire(adsrd, ADSR_ENV, vcad, VCA_CV_IN)
    p.wire(vcad, VCA_OUT, mix, MIX_IN0 + 3)
    # out
    p.wire(mix, MIX_OUT, aud, AUDIO_L)
    p.wire(mix, MIX_OUT, aud, AUDIO_R)
    p.write("patches/03_four_pages.vcv")


if __name__ == "__main__":
    os.makedirs("patches", exist_ok=True)
    patch_flin_sliders()
    patch_gome_64notes()
    patch_four_pages()
