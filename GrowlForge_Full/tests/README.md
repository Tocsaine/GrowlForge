# Regression tools

These small Linux command-line hosts compare a reference CLAP binary with a candidate build.

```bash
g++ -std=c++20 -O2 -I../external/clap/include render_clap.cpp -ldl -o render_clap
g++ -std=c++20 -O2 -I../external/clap/include state_roundtrip.cpp -ldl -o state_roundtrip
g++ -std=c++20 -O2 -I../external/clap/include render_state.cpp -ldl -o render_state
g++ -std=c++20 -O2 -I../external/clap/include inspect_clap.cpp -ldl -o inspect_clap
```

`run_regression.sh` accepts the 2.0.3 reference binary and the candidate binary.
