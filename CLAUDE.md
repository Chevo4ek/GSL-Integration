# GSL Integration — notes for Claude

Geode SDK v5.10.1 (latest bindings; BoomListType is hardcoded for 2.2081 in GSLPopup.cpp, see LEVEL_LIST) mod for Geometry Dash 2.2081 (Windows + Android64/Android32). The list id "future" is shown to players as "Upcoming Levels". Sources are in `src/`; the
copies of `.cpp/.hpp` in the repo root are old and not built.

## Rules
- **After every change, update `changelog.md`** (current version section, short bullet per change).
- Keep `about.md` in sync when features change.
- Talk to the user in simple Russian, without unnecessary technical details.
- Build is checked via GitHub Actions (`Build Geode Mod`) after each push.
