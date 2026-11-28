// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

// The pure decisions of the own-workspace feature (PlasmaZones#1201): a window
// that goes fullscreen, or that the user maximizes, moves to a temporary
// workspace of its own directly after the one it was on, and goes back when it
// leaves that state. WorkspaceController performs what these decide.

namespace PhosphorEngineRuntime::OwnWorkspaceDecisions {

/// Why a window holds an own workspace. Fullscreen wins when both hold, so a
/// fullscreen window that is also maximized keeps its workspace while either
/// option that covers it is on.
enum class Trigger {
    None,
    Fullscreen,
    Maximized,
    Borderless,
};

/// The three options, as the controller holds them.
struct Options
{
    bool fullscreen = false;
    bool maximized = false;
    bool borderless = false;

    static constexpr Options all()
    {
        return Options{true, true, true};
    }
};

/// A window's state as the options read it.
struct State
{
    /// The window's own fullscreen, not a windowed fullscreen the scrolling
    /// strip imposes.
    bool fullscreen = false;
    /// The user's own maximize, not a monocle stack's.
    bool maximized = false;
    /// No decoration, and the frame is exactly its screen's geometry.
    bool borderless = false;
};

/// The trigger for a window's state under the options, in this order:
/// fullscreen, maximized, borderless.
inline constexpr Trigger triggerFor(const Options& options, const State& state)
{
    if (options.fullscreen && state.fullscreen) {
        return Trigger::Fullscreen;
    }
    if (options.maximized && state.maximized) {
        return Trigger::Maximized;
    }
    if (options.borderless && state.borderless) {
        return Trigger::Borderless;
    }
    return Trigger::None;
}

/// The facts that decide whether a window may take an own workspace now.
struct EnterFacts
{
    /// The window is on all desktops, so a move cannot take it anywhere.
    bool sticky = false;
    /// The window counts toward exactly one desktop (a window on several
    /// desktops has no single place to come back to).
    bool singleDesktop = false;
    /// That desktop is the one its owner screen shows. A window that changes
    /// state on a workspace nobody is looking at is left where it is.
    bool originShown = false;
    /// The origin is itself an own workspace. Its window already has the
    /// workspace to itself.
    bool originIsOwnWorkspace = false;
    /// Other windows on the origin desktop. Alone, the window already has a
    /// workspace of its own and nothing moves.
    int othersOnOrigin = 0;
    /// The owner screen has an empty workspace at the end of its list. There
    /// is none at the desktop cap, and then the window stays.
    bool trailingEmpty = false;
    /// A workspace move for the window is already in flight.
    bool movePending = false;
};

inline constexpr bool mayEnter(const EnterFacts& f)
{
    return !f.sticky && f.singleDesktop && f.originShown && !f.originIsOwnWorkspace && f.othersOnOrigin > 0
        && f.trailingEmpty && !f.movePending;
}

/// What the end of an own workspace does.
enum class LeaveAction {
    /// Move the window back to its origin and show the origin, the window
    /// keeping focus. The window left the state.
    ReturnWindow,
    /// Show the origin and focus the window that was last active there. The
    /// window closed.
    ReturnScreen,
    /// Forget the record and change nothing. The window is no longer on its
    /// own workspace (the user moved it on), or its origin is gone.
    Forget,
};

/// @p closed the window closed. @p onOwnWorkspace the window is still on its
/// own workspace. @p originExists the origin desktop is still in the list.
inline constexpr LeaveAction leaveActionFor(bool closed, bool onOwnWorkspace, bool originExists)
{
    if (!originExists) {
        return LeaveAction::Forget;
    }
    if (closed) {
        return LeaveAction::ReturnScreen;
    }
    return onOwnWorkspace ? LeaveAction::ReturnWindow : LeaveAction::Forget;
}

} // namespace PhosphorEngineRuntime::OwnWorkspaceDecisions
