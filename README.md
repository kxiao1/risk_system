# Simple risk system

Input: Yield curves, FX spot rates, trade data

Output: DV01, FX sensitivity (TODO)

The idea and input data for this project come from NUS FE5226 (see https://github.com/fecpp/minirisk).

## Building

- cmake --build ./build
- cmake --build ./build --config Debug --target risk_system --
- cmake --build ./build --config Release --target all --

References:

- https://cliutils.gitlab.io/modern-cmake/chapters/intro/running.html
- https://gist.github.com/sonictk/8ce00c6019d3a19056c054e485326434

## Playing with the VSCode Addin:

- "cmake debug" builds the debug target and debugs with gdb
- pressing F7 builds default target (e.g. debug)
