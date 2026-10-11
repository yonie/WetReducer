# WetReducer - agent notes

Unreleased prototype. **No saved-state migrations until the first release** (Ronald,
2026-10-02: "this plugin does not exist yet"). Change parameters and the state format freely;
do not write code to load older prototype sessions. The line's rule that a changed control
needs a migration (hub `AGENTS.md`, settled decision 16) starts at v1.0.0 - write the
upgrade checker and pin the v1.0.0 state blobs then.

Before that release, strip the old-version branches in `setState` / `setComponentState` and
start the state format again at version 1.

## Neutral is not transparent, and that is the design (Ronald, 2026-10-11)

A bypass A/B with every control at 0 is duller, and it is not a bug. **The top band is captured
and the top slider is the filter** - without it the captured top band's sound would leak through.
Ronald, on the urgent ticket an agent raised on 2026-10-09: *"its not an issue like i thought.
its working as intended."* Ticket closed the same day.

Do not re-file this, and do not make the neutral path bypass-transparent to quieten a reviewer:
the captured top band is meant to be filtered. It lives in `source/reducerengine.h` - AIR's high
edge (`kHighEdgeDefaultNote`, E9 by default) and the band split beside it, where the music
outside an edge passes as it is.
