# cam-faceless

*The headless sibling of [MacCam-6](https://github.com/cleobuline/maccam-6): the same 1987 machine, running without a window.*

`cam-faceless` is a pure-C, Cocoa-free build of the CAM-8 engine — Tommaso Toffoli and Norman Margolus's cellular automata machine from *Cellular Automata Machines* (MIT Press, 1987) — packaged as a small command-line tool (`camgen`) that compiles a CAM-Forth rule, runs it for N steps, and writes out a sequence of PPM frames. No Xcode, no Cocoa, no macOS required: it builds and runs on plain Linux, tested end-to-end on a stock Ubuntu VPS.

This is the same engine used by the full macOS app — `cam_core.c` and `cam_forth.c` here are the **exact same files**, unmodified, that drive the windowed version. The separation between engine and interface was deliberate from the start, precisely so a headless build like this one would need almost no extra work.

## What's in this repository

```
cam_core.h   / cam_core.c    → the simulation engine: LUT (Moore/VonNeumann) and Margolus stepping,
                                 CAM-A + CAM-B, reversibility, run-cycles
cam_forth.h  / cam_forth.c   → the CAM-Forth interpreter: VM, compiler, table builders
camgen.c                     → the headless CLI front-end
regles/                      → a bestiary of ready-to-use CAM-Forth rules
```

**Not included here**: the FHP lattice-gas subsystem (`fhp.h/.c`) and anything Cocoa-specific. Those live in the [main MacCam-6 repository](https://github.com/cleobuline/maccam-6), which also has the full windowed app, the palette UI, video export, and a much longer read on the project's history and internals.

## Building

```bash
gcc -O2 -o camgen camgen.c cam_core.c cam_forth.c
```

No external dependencies. Tested with gcc on Ubuntu 22.04 and with clang on macOS — the engine code has no platform-specific calls at all.

## Options

```bash
./camgen --rule regles/hpp-gas.rule --size 256 --steps 500 --every 5 --out frames/
```

| Option | Meaning | Default |
|---|---|---|
| `--rule FILE` | path to a `.rule` file (required) | — |
| `--size N` | grid size: 128, 256, 512, or 1024 | 256 |
| `--steps N` | number of simulation steps | 200 |
| `--every N` | write a frame only every N steps | 1 |
| `--seed PCT` | random seed density, 0–100 | 20 |
| `--seed-plane N` | which plane (0–3) to seed | 0 |
| `--out DIR` | output directory for PPM frames | `frames` |
| `--quiet` | suppress progress messages | off |

## Examples

### A reversible gas (HPP-GAS, §12.3–12.4)

The classic diagonal lattice gas: two particles meeting head-on deflect onto the other diagonal, conserving both particle count and momentum exactly.

```bash
./camgen --rule regles/hpp-gas.rule --size 256 --steps 500 --every 5 --seed 30 --out frames/hpp/
```

Verified directly on this repo's code: particle count stays *identical* between the first and last frame while the gas visibly evolves — a real check, not an assumption.

### A larger grid, sparser gas, longer run

```bash
./camgen --rule regles/tm-gas.rule --size 512 --steps 2000 --every 10 --seed 15 --out frames/tmgas/
```

Bigger grids cost more per step but `camgen` has no rendering overhead to speak of — on a 256×256 grid, 500 steps finished in well under a second in testing.

### A slow, contemplative run (thin the frame rate, not the simulation)

```bash
./camgen --rule regles/diffusion.rule --size 256 --steps 5000 --every 25 --seed 40 --out frames/diff/
```

`--every` lets the simulation itself run at full resolution in time while the output video stays a manageable length — 5000 steps sampled every 25 gives 200 frames, about 6.5 seconds at 30fps.

### Ising / Q2R, energy-conserving spin dynamics (§17)

```bash
./camgen --rule regles/ising.rule --size 256 --steps 1000 --every 4 --seed 50 --out frames/ising/
```

### Probabilistic decay (chapter 8, `RAND`-driven)

```bash
./camgen --rule regles/decay.rule --size 256 --steps 300 --every 3 --seed 60 --out frames/decay/
```

### Turning frames into video

`ffmpeg` isn't a dependency of `camgen` itself, but it's the natural next step and was used to validate the full pipeline end to end on a real VPS:

```bash
ffmpeg -framerate 30 -i frames/hpp/frame_%06d.ppm -c:v libx264 -pix_fmt yuv420p out.mp4
```

Confirmed working: 101 frames at 256×256 encoded to a 2.5MB H.264 file in about 3 seconds on an Ubuntu 22.04 VPS.

## What works today

Any rule that only needs a **uniform random seed on one plane** runs correctly out of the box — that covers most of the LUT and Margolus rules in the bestiary (gases, diffusion, spin models, probabilistic decay).

## Known limitation

`camgen` can currently only seed **one plane, uniformly at random**. Rules that need a *precise single-point seed on a specific plane, distinct from a randomly-seeded gas plane* — such as `dendrite.rule` (diffusion-limited aggregation, §15.7) — will **not** grow correctly: the freeze condition never triggers, and the gas just diffuses forever without ever crystallizing. This was verified directly: population stays frozen at an identical pixel count across every frame, proving the seed point never gets planted.

A `--seed-point x,y,plane` option to paint a single pixel before the simulation starts would fix this, and is the natural next step.

## The wider project

`cam-faceless` is one branch of a larger effort: a faithful, rigorously-tested reconstruction of the CAM-8 machine, originally the resurrection of a 1990s MacCam written in THINK C on a 68000 Mac. The full story — the CAM-Forth language, all the neighborhoods (including a pseudo-hexagonal one not in the book), the FHP gas, reversibility, the rule bestiary — lives in the [MacCam-6 README](https://github.com/cleobuline/maccam-6/blob/main/README.md).

---

*Developed with Claude (Anthropic) as a technical implementation partner.*
