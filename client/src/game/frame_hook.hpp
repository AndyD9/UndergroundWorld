#pragma once
namespace ugworld {
// Redirects the rel32 of CALL World::Tick (static-pass-2.md 3.1) to a post-tick hook.
// Refuses on an unexpected image base or bytes. Never removed: the module is pinned.
bool install_frame_hook() noexcept;
// True while the call site still targets our hook (another mod may patch it later).
bool frame_hook_intact() noexcept;
}
