# WetReducer - agent notes

Unreleased prototype. **No saved-state migrations until the first release** (Ronald,
2026-10-02: "this plugin does not exist yet"). Change parameters and the state format freely;
do not write code to load older prototype sessions. The line's rule that a changed control
needs a migration (hub `AGENTS.md`, settled decision 16) starts at v1.0.0 - write the
upgrade checker and pin the v1.0.0 state blobs then.

Before that release, strip the old-version branches in `setState` / `setComponentState` and
start the state format again at version 1.
