# GSL Integration — notes for Claude

Geode SDK v5.11.2 (bindings pinned to 1605f500 in build.yml) mod for Geometry Dash 2.2081 (Windows + Android64/Android32). The list id "future" is shown to players as "Upcoming Levels". Sources are in `src/`; the
copies of `.cpp/.hpp` in the repo root are old and not built.

## Rules
- **After every change, update `changelog.md`** (current version section, short bullet per change).
- Keep `about.md` in sync when features change.
- Talk to the user in simple Russian, without unnecessary technical details.
- Build is checked via GitHub Actions (`Build Geode Mod`) after each push.
