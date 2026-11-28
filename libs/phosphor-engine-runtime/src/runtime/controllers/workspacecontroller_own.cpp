// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

// Own workspaces (PlasmaZones#1201): a window that goes fullscreen, or that the
// user maximizes, moves to a temporary workspace of its own directly after the
// one it was on, and goes back when it leaves that state.
//
// The workspace is the screen's trailing empty one. The window moves there,
// which makes the reconciler cut a new trailing empty, and on arrival the
// filled one is moved in the map to sit after the origin. When the window
// leaves the state it moves back and the screen shows the origin again; the
// emptied workspace then goes by the ordinary destroy-on-empty. Every switch
// here is the daemon's own (Correction), so it plays no desktop-switch
// transition and raises no OSD.

#include <PhosphorEngineRuntime/WorkspaceController.h>

#include "ownworkspacedecisions.h"

#include <PhosphorEngine/WindowRegistry.h>
#include <PhosphorWorkspaces/VirtualDesktopManager.h>

#include <PhosphorPaths/SuiteDirectories.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(lcWorkspaceCtl)

namespace PhosphorEngineRuntime {

namespace OWD = OwnWorkspaceDecisions;

void WorkspaceController::setOwnWorkspaceStateProvider(
    std::function<OwnWorkspaceWindowState(const QString& instanceId)> provider)
{
    m_ownWorkspaceState = std::move(provider);
}

void WorkspaceController::setOwnWorkspaceHoldHandler(std::function<void(const QString& instanceId, bool hold)> handler)
{
    m_ownWorkspaceHold = std::move(handler);
}

void WorkspaceController::setOwnWorkspaceHoldCodec(
    std::function<QJsonObject(const QString& instanceId)> exportHolds,
    std::function<void(const QString& instanceId, const QJsonObject& holds)> importHolds)
{
    m_ownWorkspaceHoldExport = std::move(exportHolds);
    m_ownWorkspaceHoldImport = std::move(importHolds);
}

QString WorkspaceController::ownWorkspacesFilePath()
{
    return PhosphorPaths::appStateDir() + QStringLiteral("/own-workspaces.json");
}

void WorkspaceController::saveOwnWorkspaces() const
{
    QJsonArray records;
    for (auto it = m_ownWorkspaces.constBegin(); it != m_ownWorkspaces.constEnd(); ++it) {
        QJsonObject obj;
        obj[QLatin1String("window")] = it.key();
        obj[QLatin1String("screen")] = it->screenId;
        obj[QLatin1String("origin")] = it->originDesktopId;
        obj[QLatin1String("own")] = it->ownDesktopId;
        obj[QLatin1String("lastActive")] = it->originLastActive;
        obj[QLatin1String("arrived")] = it->arrived;
        if (m_ownWorkspaceHoldExport) {
            obj[QLatin1String("holds")] = m_ownWorkspaceHoldExport(it.key());
        }
        records.append(obj);
    }
    const QString path = ownWorkspacesFilePath();
    if (records.isEmpty()) {
        QFile::remove(path);
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcWorkspaceCtl) << "cannot write own-workspace state file" << path;
        return;
    }
    QJsonObject root;
    root[QLatin1String("version")] = 1;
    root[QLatin1String("records")] = records;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        qCWarning(lcWorkspaceCtl) << "could not commit own-workspace state file" << path << ":" << file.errorString();
    }
}

void WorkspaceController::loadOwnWorkspaces()
{
    QFile file(ownWorkspacesFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(QLatin1String("version")).toInt() != 1) {
        return;
    }
    for (const QJsonValue& v : root.value(QLatin1String("records")).toArray()) {
        const QJsonObject obj = v.toObject();
        const QString window = obj.value(QLatin1String("window")).toString();
        OwnWorkspace record;
        record.screenId = obj.value(QLatin1String("screen")).toString();
        record.originDesktopId = obj.value(QLatin1String("origin")).toString();
        record.ownDesktopId = obj.value(QLatin1String("own")).toString();
        record.originLastActive = obj.value(QLatin1String("lastActive")).toString();
        record.arrived = obj.value(QLatin1String("arrived")).toBool(false);
        if (window.isEmpty() || record.screenId.isEmpty() || record.originDesktopId.isEmpty()
            || record.ownDesktopId.isEmpty()) {
            continue;
        }
        m_ownWorkspaces.insert(window, record);
        if (m_ownWorkspaceHoldImport) {
            m_ownWorkspaceHoldImport(window, obj.value(QLatin1String("holds")).toObject());
        }
    }
    if (!m_ownWorkspaces.isEmpty()) {
        qCInfo(lcWorkspaceCtl) << "restored" << m_ownWorkspaces.size() << "own workspace(s) from the last run";
        // The compositor re-announces its windows after the daemon is up. A
        // record whose window has not come back by then belongs to a window
        // that closed while the daemon was down.
        QTimer::singleShot(OwnWorkspacePruneDelayMs, this, &WorkspaceController::pruneAbsentOwnWorkspaces);
    }
}

void WorkspaceController::pruneAbsentOwnWorkspaces()
{
    bool changed = false;
    for (auto it = m_ownWorkspaces.begin(); it != m_ownWorkspaces.end();) {
        if (m_registry->metadata(it.key())) {
            ++it;
            continue;
        }
        if (m_ownWorkspaceHold) {
            m_ownWorkspaceHold(it.key(), false);
        }
        it = m_ownWorkspaces.erase(it);
        changed = true;
    }
    if (changed) {
        saveOwnWorkspaces();
    }
}

void WorkspaceController::setOwnWorkspaceOptions(bool fullscreen, bool maximized, bool borderless)
{
    if (m_ownFullscreen == fullscreen && m_ownMaximized == maximized && m_ownBorderless == borderless) {
        return;
    }
    m_ownFullscreen = fullscreen;
    m_ownMaximized = maximized;
    m_ownBorderless = borderless;
    // An option turned on applies to a window already in the state the next
    // time its state changes, not at once: a switch the user did not cause
    // would land under them. An option turned off ends nothing either.
}

void WorkspaceController::noteWindowActivated(const QString& instanceId)
{
    const QStringList desktops = m_windowCensusDesktopIds.value(instanceId);
    if (desktops.size() == 1) {
        m_lastActiveByDesktop.insert(desktops.first(), instanceId);
    }
}

void WorkspaceController::noteWindowStateChanged(const QString& instanceId)
{
    if (m_registry->metadata(instanceId)) {
        evaluateOwnWorkspace(instanceId);
    }
}

QString WorkspaceController::ownWorkspaceOf(const QString& instanceId) const
{
    const auto it = m_ownWorkspaces.constFind(instanceId);
    return it == m_ownWorkspaces.constEnd() ? QString() : it->ownDesktopId;
}

int WorkspaceController::othersOnDesktop(const QString& desktopId, const QString& exceptInstanceId) const
{
    int count = 0;
    for (auto it = m_windowCensusDesktopIds.constBegin(); it != m_windowCensusDesktopIds.constEnd(); ++it) {
        if (it.key() != exceptInstanceId && it.value().contains(desktopId)) {
            ++count;
        }
    }
    return count;
}

void WorkspaceController::evaluateOwnWorkspace(const QString& instanceId)
{
    if (!m_ownWorkspaceState || !m_adopted) {
        return;
    }
    const OwnWorkspaceWindowState state = m_ownWorkspaceState(instanceId);
    // A window that holds an own workspace keeps it while any of the states
    // holds (the options are read as all on for it).
    const OWD::Options options = m_ownWorkspaces.contains(instanceId)
        ? OWD::Options::all()
        : OWD::Options{m_ownFullscreen, m_ownMaximized, m_ownBorderless};
    const OWD::Trigger trigger =
        OWD::triggerFor(options, OWD::State{state.fullscreen, state.maximized, state.borderless});
    if (m_ownWorkspaces.contains(instanceId)) {
        // A window that holds an own workspace keeps it while either state
        // holds (the options are read as on for it), so leaving fullscreen
        // into a maximize does not bounce the window back and out again.
        if (trigger == OWD::Trigger::None) {
            leaveOwnWorkspace(instanceId, /*closed=*/false);
        }
        return;
    }
    if (trigger != OWD::Trigger::None) {
        enterOwnWorkspace(instanceId);
    }
}

void WorkspaceController::enterOwnWorkspace(const QString& instanceId)
{
    if (m_ownWorkspaceEnterPending.contains(instanceId)) {
        return;
    }
    m_ownWorkspaceEnterPending.insert(instanceId);
    // Deferred behind structural churn, like every verb here: the trailing
    // empty and the slice indices must be the settled ones.
    runWhenQuiet([this, instanceId]() {
        m_ownWorkspaceEnterPending.remove(instanceId);
        if (!m_registry->metadata(instanceId) || m_ownWorkspaces.contains(instanceId) || !m_ownWorkspaceState) {
            return;
        }
        const OwnWorkspaceWindowState state = m_ownWorkspaceState(instanceId);
        if (OWD::triggerFor(OWD::Options{m_ownFullscreen, m_ownMaximized, m_ownBorderless},
                            OWD::State{state.fullscreen, state.maximized, state.borderless})
            == OWD::Trigger::None) {
            return;
        }
        const QStringList desktops = m_windowCensusDesktopIds.value(instanceId);
        const QString origin = desktops.size() == 1 ? desktops.first() : QString();
        const QString screenId = origin.isEmpty() ? QString() : m_reconciler.map().ownerOf(origin);
        const QList<PhosphorWorkspaces::WorkspaceEntry> slice =
            screenId.isEmpty() ? QList<PhosphorWorkspaces::WorkspaceEntry>() : m_reconciler.map().slice(screenId);
        const QString trailing = slice.isEmpty() ? QString() : slice.last().desktopId;
        bool originIsOwn = false;
        for (const OwnWorkspace& record : std::as_const(m_ownWorkspaces)) {
            originIsOwn = originIsOwn || record.ownDesktopId == origin;
        }
        OWD::EnterFacts facts;
        facts.sticky = isStickyWindow(instanceId);
        facts.singleDesktop = !origin.isEmpty();
        facts.originShown = !screenId.isEmpty() && m_reconciler.currentDesktopIdOf(screenId) == origin;
        facts.originIsOwnWorkspace = originIsOwn;
        facts.othersOnOrigin = origin.isEmpty() ? 0 : othersOnDesktop(origin, instanceId);
        facts.trailingEmpty = !trailing.isEmpty() && trailing != origin && m_populationById.value(trailing, 0) == 0;
        facts.movePending = m_pendingWindowMoves.contains(instanceId);
        if (!OWD::mayEnter(facts)) {
            qCDebug(lcWorkspaceCtl) << "own workspace for window" << instanceId << "not opened: sticky" << facts.sticky
                                    << "single desktop" << facts.singleDesktop << "origin shown" << facts.originShown
                                    << "origin is own" << facts.originIsOwnWorkspace << "others" << facts.othersOnOrigin
                                    << "trailing empty" << facts.trailingEmpty << "move pending" << facts.movePending;
            return;
        }
        const int desktop = m_vdm->desktopIndexOf(trailing);
        if (desktop <= 0) {
            return;
        }
        OwnWorkspace record;
        record.screenId = screenId;
        record.originDesktopId = origin;
        record.ownDesktopId = trailing;
        const QString lastActive = m_lastActiveByDesktop.value(origin);
        record.originLastActive = lastActive == instanceId ? QString() : lastActive;
        m_ownWorkspaces.insert(instanceId, record);
        saveOwnWorkspaces();
        qCInfo(lcWorkspaceCtl) << "opening own workspace" << trailing << "for window" << instanceId << "from" << origin
                               << "on" << screenId;
        // The engine keeps the window's place and its maximize first, since
        // the move's handoff releases both.
        if (m_ownWorkspaceHold) {
            m_ownWorkspaceHold(instanceId, true);
            saveOwnWorkspaces();
        }
        // Not the subject of a verb, so the move leaves the window's
        // fullscreen alone, and the desktop half only: the own workspace is on
        // the window's monitor.
        issueWindowMove(instanceId, screenId, desktop, trailing, QStringLiteral("down"), /*moveOutput=*/false,
                        /*subjectOfVerb=*/false);
        switchScreenToDesktop(screenId, trailing, PhosphorProtocol::ScreenDesktopSwitchPurpose::Correction);
    });
}

void WorkspaceController::noteOwnWorkspaceCensus(const QString& instanceId)
{
    const auto it = m_ownWorkspaces.find(instanceId);
    if (it == m_ownWorkspaces.end()) {
        return;
    }
    const QStringList desktops = m_windowCensusDesktopIds.value(instanceId);
    if (desktops.contains(it->ownDesktopId)) {
        if (it->arrived) {
            return;
        }
        it->arrived = true;
        saveOwnWorkspaces();
        const QString own = it->ownDesktopId;
        const QString origin = it->originDesktopId;
        // Placed after the origin once the arrival has cut the new trailing
        // empty and the list has settled. Map order only: KWin's global order
        // is not rewritten.
        runWhenQuiet([this, own, origin]() {
            const int originIndex = m_reconciler.map().sliceIndexOf(origin);
            if (originIndex >= 0 && m_reconciler.map().ownerOf(own) == m_reconciler.map().ownerOf(origin)) {
                m_reconciler.reorderWorkspace(own, originIndex + 1);
            }
        });
        return;
    }
    // Not there (yet). Before the arrival this is the move still on its way.
    // After it, the window went somewhere else: the user moved it on, and the
    // workspace is an ordinary one from now on.
    if (it->arrived && !desktops.isEmpty()) {
        qCInfo(lcWorkspaceCtl) << "window" << instanceId << "left its own workspace" << it->ownDesktopId
                               << "— forgetting the record";
        m_ownWorkspaces.erase(it);
        if (m_ownWorkspaceHold) {
            m_ownWorkspaceHold(instanceId, false);
        }
        saveOwnWorkspaces();
    }
}

void WorkspaceController::noteOwnWorkspaceVisitor(const QString& instanceId)
{
    if (!m_adopted || m_ownWorkspaces.isEmpty() || m_ownWorkspaces.contains(instanceId)
        || m_ownWorkspaceVisitors.contains(instanceId)) {
        return;
    }
    const auto meta = m_registry->metadata(instanceId);
    // A dialog, a notification or a helper window that keeps out of the task
    // bar belongs with the window it serves: a game's own dialogs stay with
    // the game.
    if (!meta || meta->isTransient.value_or(false) || meta->isNotification.value_or(false)
        || meta->skipTaskbar.value_or(false)) {
        return;
    }
    const QStringList desktops = m_windowCensusDesktopIds.value(instanceId);
    if (desktops.size() != 1) {
        return;
    }
    QString owner;
    for (auto it = m_ownWorkspaces.constBegin(); it != m_ownWorkspaces.constEnd(); ++it) {
        if (it->arrived && it->ownDesktopId == desktops.first()) {
            owner = it.key();
            break;
        }
    }
    if (owner.isEmpty()) {
        return;
    }
    // A window that opens in a state of its own (fullscreen itself) is no
    // visitor: it takes an own workspace of its own once it can.
    if (m_ownWorkspaceState) {
        const OwnWorkspaceWindowState state = m_ownWorkspaceState(instanceId);
        if (OWD::triggerFor(OWD::Options{m_ownFullscreen, m_ownMaximized, m_ownBorderless},
                            OWD::State{state.fullscreen, state.maximized, state.borderless})
            != OWD::Trigger::None) {
            return;
        }
    }
    const OwnWorkspace& record = m_ownWorkspaces[owner];
    OwnWorkspaceVisitor visit{record.screenId, record.originDesktopId, record.ownDesktopId, owner};
    runWhenQuiet([this, instanceId, visit]() {
        const int originDesktop = m_vdm->desktopIndexOf(visit.originDesktopId);
        if (originDesktop <= 0 || !m_registry->metadata(instanceId)
            || !m_windowCensusDesktopIds.value(instanceId).contains(visit.ownDesktopId)) {
            return;
        }
        qCInfo(lcWorkspaceCtl) << "window" << instanceId << "opened on the own workspace of" << visit.owner
                               << "— moving it to" << visit.originDesktopId;
        m_ownWorkspaceVisitors.insert(instanceId, visit);
        issueWindowMove(instanceId, visit.screenId, originDesktop, visit.originDesktopId, QStringLiteral("up"),
                        /*moveOutput=*/false, /*subjectOfVerb=*/false);
        if (m_reconciler.currentDesktopIdOf(visit.screenId) == visit.ownDesktopId) {
            switchScreenToDesktop(visit.screenId, visit.originDesktopId,
                                  PhosphorProtocol::ScreenDesktopSwitchPurpose::Correction);
        }
        Q_EMIT windowActivationRequested(instanceId);
    });
}

void WorkspaceController::endOwnWorkspaceVisit(const QString& instanceId)
{
    const auto it = m_ownWorkspaceVisitors.constFind(instanceId);
    if (it == m_ownWorkspaceVisitors.constEnd()) {
        return;
    }
    const OwnWorkspaceVisitor visit = *it;
    m_ownWorkspaceVisitors.erase(it);
    const auto owner = m_ownWorkspaces.constFind(visit.owner);
    // Only while the screen still shows the origin the visit took it to, and
    // the own workspace it left is still the owner's.
    if (owner == m_ownWorkspaces.constEnd() || owner->ownDesktopId != visit.ownDesktopId
        || m_reconciler.currentDesktopIdOf(visit.screenId) != visit.originDesktopId
        || m_vdm->desktopIndexOf(visit.ownDesktopId) <= 0) {
        return;
    }
    qCInfo(lcWorkspaceCtl) << "visitor" << instanceId << "closed — back to the own workspace of" << visit.owner;
    switchScreenToDesktop(visit.screenId, visit.ownDesktopId, PhosphorProtocol::ScreenDesktopSwitchPurpose::Correction);
    Q_EMIT windowActivationRequested(visit.owner);
}

void WorkspaceController::leaveOwnWorkspace(const QString& instanceId, bool closed)
{
    const OwnWorkspace record = m_ownWorkspaces.take(instanceId);
    // Its visitors have no own workspace to go back to any more.
    for (auto it = m_ownWorkspaceVisitors.begin(); it != m_ownWorkspaceVisitors.end();) {
        it = it->owner == instanceId ? m_ownWorkspaceVisitors.erase(it) : std::next(it);
    }
    saveOwnWorkspaces();
    const int originDesktop = m_vdm->desktopIndexOf(record.originDesktopId);
    // On its own workspace, or on its way there: a state that ends before the
    // move lands still owes the way back.
    const bool onOwn = m_windowCensusDesktopIds.value(instanceId).contains(record.ownDesktopId)
        || m_pendingWindowMoves.value(instanceId) == record.ownDesktopId;
    const bool showsOwn = m_reconciler.currentDesktopIdOf(record.screenId) == record.ownDesktopId;
    switch (OWD::leaveActionFor(closed, onOwn, originDesktop > 0)) {
    case OWD::LeaveAction::ReturnWindow:
        qCInfo(lcWorkspaceCtl) << "closing own workspace" << record.ownDesktopId << "of window" << instanceId
                               << "— back to" << record.originDesktopId;
        issueWindowMove(instanceId, record.screenId, originDesktop, record.originDesktopId, QStringLiteral("up"),
                        /*moveOutput=*/false, /*subjectOfVerb=*/false);
        if (showsOwn) {
            switchScreenToDesktop(record.screenId, record.originDesktopId,
                                  PhosphorProtocol::ScreenDesktopSwitchPurpose::Correction);
        }
        Q_EMIT windowActivationRequested(instanceId);
        return;
    case OWD::LeaveAction::ReturnScreen:
        qCInfo(lcWorkspaceCtl) << "window" << instanceId << "closed on its own workspace" << record.ownDesktopId
                               << "— back to" << record.originDesktopId;
        if (!showsOwn) {
            return;
        }
        switchScreenToDesktop(record.screenId, record.originDesktopId,
                              PhosphorProtocol::ScreenDesktopSwitchPurpose::Correction);
        if (!record.originLastActive.isEmpty()
            && m_windowCensusDesktopIds.value(record.originLastActive).contains(record.originDesktopId)) {
            Q_EMIT windowActivationRequested(record.originLastActive);
        }
        return;
    case OWD::LeaveAction::Forget:
        if (m_ownWorkspaceHold) {
            m_ownWorkspaceHold(instanceId, false);
        }
        return;
    }
}

} // namespace PhosphorEngineRuntime
