# Claude Code Notes

## Building

Always build from the build directory:

```bash
cd /home/josh/FaceOfBattle/build && make -j$(nproc)
```

Or if CMakeLists.txt changed:

```bash
cd /home/josh/FaceOfBattle/build && cmake .. && make -j$(nproc)
```

Do NOT run `make` from the project root - there's no Makefile there.
