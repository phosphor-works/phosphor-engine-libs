// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

// The compositor's fullscreen hold (ScrollEngine::setWindowFullscreenFloat):
// a strip tile entering its OWN fullscreen keeps its column, so no other
// column moves while it covers its output (#1266). The hold is not a float:
// nothing is announced, the strip is not laid out again, and the end of the
// hold finds the tile where it was. The hold never takes over or undoes a user
// float, keeps an opening window out of the held column, keeps answering
// through a close until the daemon's capture has read it, and ends on a
// re-announce when the effect has lost its record (a restart).
// Its own file rather than more of the maximize suite that holds the other
// fullscreen arms, since the hold has its own contract.

#include <PhosphorScrollEngine/ScrollEngine.h>
#include <PhosphorScrollEngine/ScrollState.h>

#include <PhosphorEngine/WindowPlacement.h>

#include "scrollstriptestutils.h"
#include "scrollstubsettings.h"

#include <QSignalSpy>
#include <QtTest>

using namespace PhosphorScrollEngine;

using ScrollTestUtils::makeProviderEngine;
using ScrollTestUtils::StubScrollSettings;

namespace {

const QString kScreen = QStringLiteral("S1");
const QString kA = QStringLiteral("app|a");
const QString kB = QStringLiteral("app|b");
const QString kC = QStringLiteral("app|c");

ScrollState* stateFor(ScrollEngine* engine)
{
    return static_cast<ScrollState*>(engine->stateForScreen(kScreen));
}

/// Three single-tile columns a, b, c with b focused.
ScrollEngine* threeColumns(QObject* owner)
{
    ScrollEngine* engine = makeProviderEngine(owner, {kScreen});
    for (const QString& id : {kA, kB, kC}) {
        engine->windowOpened(id, kScreen, 0, 0);
    }
    engine->windowFocused(kB, kScreen);
    QCoreApplication::processEvents();
    return engine;
}

/// Each column's window ids, in strip order.
QList<QStringList> columnsOf(const ScrollState* state)
{
    QList<QStringList> out;
    for (const Column& column : state->strip().columns()) {
        QStringList ids;
        for (const Tile& tile : column.tiles) {
            ids.append(tile.windowId);
        }
        out.append(ids);
    }
    return out;
}

} // namespace

class TestScrollEngineFullscreenFloat : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    /// Proves the vertical arm really is transposed, so a lost ENVIRONMENT
    /// property cannot leave it silently re-running the horizontal suite.
    void initTestCase()
    {
        AX_GUARD_SUITE();
    }

    void theHoldKeepsTheColumnAndMovesNoNeighbour();
    void aSecondHoldAndReturnCycleKeepsTheSlot();
    void aUserFloatIsNeitherTakenOverNorUndone();
    void aUserFloatDuringTheHoldEndsIt();
    void aReAnnounceDuringTheHoldEndsIt();
    void anUntrackedWindowIsRefused();
    void anOpeningWindowDoesNotJoinTheHeldColumn();
    void aSameAppOpenDoesNotJoinTheHeldColumnAsATab();
    void aHeldTabStaysInItsTabGroup();
    void aHeldWindowIsCapturedTiled();
    void aCommittedSizeReportDuringTheHoldIsIgnored();
    void aCloseDuringTheHoldKeepsTheHoldAnswerUntilThePrune();
    void aReopenOfAClosedHeldWindowConsumesTheHoldAnswer();
    void aModeReleaseEndsTheHoldWithoutAFloatAnnouncement();
};

void TestScrollEngineFullscreenFloat::theHoldKeepsTheColumnAndMovesNoNeighbour()
{
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    ScrollState* state = stateFor(engine);
    QVERIFY(state);
    // A width the context default cannot produce, so the end of the hold
    // proves the column kept it.
    engine->setColumnWidth(ColumnWidth::makeFixed(377), kScreen);
    QCoreApplication::processEvents();
    const QList<QStringList> before = columnsOf(state);
    const int anchorBefore = state->strip().viewAnchor();

    QSignalSpy passive(engine, &PhosphorEngine::PlacementEngineBase::windowFloatingStateSynced);
    QSignalSpy active(engine, &PhosphorEngine::PlacementEngineBase::windowFloatingChanged);
    QSignalSpy tiled(engine, &ScrollEngine::windowsTiled);

    // A foreign screen hint: the engine acts on the window's own screen.
    QVERIFY(engine->setWindowFullscreenFloat(kB, true, QStringLiteral("S9")));
    QCoreApplication::processEvents();
    QVERIFY(engine->isFullscreenHeld(kB));
    QVERIFY(!state->isFloating(kB));
    // The strip is as it was: the same columns, the same view, the same focus.
    // Taking the tile out let the next column slide into its slot (#1266).
    QCOMPARE(columnsOf(state), before);
    QCOMPARE(state->strip().viewAnchor(), anchorBefore);
    QCOMPARE(state->strip().activeWindowId(), kB);
    QCOMPARE(state->strip().columns().at(1).width, ColumnWidth::makeFixed(377));
    // Nothing is announced and no batch goes out: no neighbour gets a rect.
    QCOMPARE(passive.count(), 0);
    QCOMPARE(active.count(), 0);
    QCOMPARE(tiled.count(), 0);
    // A repeat hold answers true and changes nothing: the effect re-enters
    // after a lost reply and must not read "refused" as "not ours".
    QVERIFY(engine->setWindowFullscreenFloat(kB, true, kScreen));
    QVERIFY(engine->isFullscreenHeld(kB));

    QVERIFY(engine->setWindowFullscreenFloat(kB, false, kScreen));
    QCoreApplication::processEvents();
    QVERIFY(!engine->isFullscreenHeld(kB));
    QVERIFY(!state->isFloating(kB));
    QCOMPARE(columnsOf(state), before);
    QCOMPARE(state->strip().columns().at(1).width, ColumnWidth::makeFixed(377));
    QCOMPARE(passive.count(), 0);
    QCOMPARE(active.count(), 0);
    // The end of the hold has nothing left to end a second time.
    QVERIFY(!engine->setWindowFullscreenFloat(kB, false, kScreen));
}

void TestScrollEngineFullscreenFloat::aSecondHoldAndReturnCycleKeepsTheSlot()
{
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    ScrollState* state = stateFor(engine);
    QVERIFY(state);
    const QList<QStringList> before = columnsOf(state);

    for (int cycle = 0; cycle < 2; ++cycle) {
        QVERIFY2(engine->setWindowFullscreenFloat(kB, true, kScreen),
                 qPrintable(QStringLiteral("hold, cycle %1").arg(cycle)));
        QVERIFY(engine->isFullscreenHeld(kB));
        QCOMPARE(columnsOf(state), before);
        QVERIFY2(engine->setWindowFullscreenFloat(kB, false, kScreen),
                 qPrintable(QStringLiteral("return, cycle %1").arg(cycle)));
        QCoreApplication::processEvents();
        QCOMPARE(columnsOf(state), before);
        QVERIFY(!engine->isFullscreenHeld(kB));
    }
    QVERIFY(!engine->setWindowFullscreenFloat(kB, false, kScreen));
}

void TestScrollEngineFullscreenFloat::aUserFloatIsNeitherTakenOverNorUndone()
{
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    ScrollState* state = stateFor(engine);
    QVERIFY(state);

    engine->setWindowFloat(kB, true, kScreen);
    QVERIFY(state->isFloating(kB));
    QVERIFY(!engine->setWindowFullscreenFloat(kB, true, kScreen));
    QVERIFY(!engine->isFullscreenHeld(kB));
    QVERIFY(!engine->setWindowFullscreenFloat(kB, false, kScreen));
    QVERIFY(state->isFloating(kB));
}

void TestScrollEngineFullscreenFloat::aUserFloatDuringTheHoldEndsIt()
{
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    ScrollState* state = stateFor(engine);
    QVERIFY(state);

    QVERIFY(engine->setWindowFullscreenFloat(kB, true, kScreen));
    engine->setWindowFloat(kB, true, kScreen);
    QVERIFY(state->isFloating(kB));
    QVERIFY(!engine->isFullscreenHeld(kB));
    // The compositor's later return finds no hold of its own to end, and the
    // user's float stands.
    QVERIFY(!engine->setWindowFullscreenFloat(kB, false, kScreen));
    QVERIFY(state->isFloating(kB));
}

void TestScrollEngineFullscreenFloat::aReAnnounceDuringTheHoldEndsIt()
{
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    ScrollState* state = stateFor(engine);
    QVERIFY(state);
    const QList<QStringList> before = columnsOf(state);

    QVERIFY(engine->setWindowFullscreenFloat(kB, true, kScreen));
    // An effect that lost its record of the hold (a restart) announces the
    // window when it leaves fullscreen. The tile never left its column.
    engine->windowOpened(kB, kScreen, 0, 0);
    QCoreApplication::processEvents();
    QVERIFY(!engine->isFullscreenHeld(kB));
    QVERIFY(!state->isFloating(kB));
    QCOMPARE(columnsOf(state), before);
}

void TestScrollEngineFullscreenFloat::anUntrackedWindowIsRefused()
{
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    QVERIFY(!engine->setWindowFullscreenFloat(QStringLiteral("app|zzz"), true, kScreen));
    QVERIFY(!engine->setWindowFullscreenFloat(QStringLiteral("app|zzz"), false, kScreen));
    QVERIFY(!engine->isFullscreenHeld(QStringLiteral("app|zzz")));
}

void TestScrollEngineFullscreenFloat::anOpeningWindowDoesNotJoinTheHeldColumn()
{
    // A consume rule puts an opening window into the active column. While
    // that column is fullscreen, the window takes a column of its own.
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    ScrollState* state = stateFor(engine);
    QVERIFY(state);
    engine->setOpenParamsResolver([](const QString&, const QString&) {
        ScrollOpenParams params;
        params.consume = true;
        return params;
    });
    QVERIFY(engine->setWindowFullscreenFloat(kB, true, kScreen));
    const QString opened = QStringLiteral("other|d");
    engine->windowOpened(opened, kScreen, 0, 0);
    QCoreApplication::processEvents();
    QCOMPARE(state->strip().columns().at(state->strip().columnOfWindow(kB)).tiles.size(), 1);
    QVERIFY(state->strip().columnOfWindow(opened) >= 0);
    QVERIFY(state->strip().columnOfWindow(opened) != state->strip().columnOfWindow(kB));
    QCOMPARE(state->strip().columnCount(), 4);

    // Once the hold ends, the rule joins the active column again.
    QVERIFY(engine->setWindowFullscreenFloat(kB, false, kScreen));
    engine->windowFocused(kB, kScreen);
    const QString later = QStringLiteral("other|e");
    engine->windowOpened(later, kScreen, 0, 0);
    QCoreApplication::processEvents();
    QCOMPARE(state->strip().columnOfWindow(later), state->strip().columnOfWindow(kB));
}

void TestScrollEngineFullscreenFloat::aSameAppOpenDoesNotJoinTheHeldColumnAsATab()
{
    // The launcher-plus-game shape: with same-app tabs on, a game that shares
    // its launcher's app id would open as a tab in the launcher's column.
    // While that column is fullscreen, it opens in a column of its own.
    QObject owner;
    auto* settings = new StubScrollSettings(&owner);
    settings->groupSameAppAsTabs = true;
    ScrollEngine* engine = makeProviderEngine(&owner, {kScreen});
    engine->setEngineSettings(settings);
    engine->refreshConfigFromSettings();
    engine->windowOpened(kA, kScreen, 0, 0);
    engine->windowOpened(QStringLiteral("other|x"), kScreen, 0, 0);
    engine->windowFocused(kA, kScreen);
    QCoreApplication::processEvents();
    ScrollState* state = stateFor(engine);
    QVERIFY(state);
    QCOMPARE(state->strip().columnCount(), 2);

    QVERIFY(engine->setWindowFullscreenFloat(kA, true, kScreen));
    engine->windowOpened(kB, kScreen, 0, 0);
    QCoreApplication::processEvents();
    QCOMPARE(state->strip().columnCount(), 3);
    QVERIFY(state->strip().columnOfWindow(kB) != state->strip().columnOfWindow(kA));
}

void TestScrollEngineFullscreenFloat::aHeldTabStaysInItsTabGroup()
{
    QObject owner;
    ScrollEngine* engine = makeProviderEngine(&owner, {kScreen});
    engine->windowOpened(kA, kScreen, 0, 0);
    engine->windowOpened(kB, kScreen, 0, 0);
    engine->windowFocused(kA, kScreen);
    engine->consumeWindowIntoColumn(kScreen);
    engine->toggleColumnTabbed(kScreen);
    engine->windowFocused(kB, kScreen);
    QCoreApplication::processEvents();
    ScrollState* state = stateFor(engine);
    QVERIFY(state);
    QCOMPARE(state->strip().columnCount(), 1);
    QCOMPARE(state->strip().columns().first().display, ColumnDisplay::Tabbed);

    QVERIFY(engine->setWindowFullscreenFloat(kB, true, kScreen));
    QCOMPARE(state->strip().columnCount(), 1);
    QCOMPARE(state->strip().columnOfWindow(kB), state->strip().columnOfWindow(kA));
    QCOMPARE(state->strip().activeWindowId(), kB);

    QVERIFY(engine->setWindowFullscreenFloat(kB, false, kScreen));
    QCoreApplication::processEvents();
    QCOMPARE(state->strip().columnCount(), 1);
    QCOMPARE(state->strip().columnOfWindow(kB), state->strip().columnOfWindow(kA));
    QCOMPARE(state->strip().columns().first().display, ColumnDisplay::Tabbed);
    QCOMPARE(state->strip().activeWindowId(), kB);
}

void TestScrollEngineFullscreenFloat::aHeldWindowIsCapturedTiled()
{
    // The hold is not a float, so a capture during it records the column the
    // window keeps, and a reopen or a mode round trip tiles it.
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    QVERIFY(engine->setWindowFullscreenFloat(kB, true, kScreen));
    const auto during = engine->capturePlacement(kB);
    QVERIFY(during.has_value());
    QCOMPARE(during->engines.value(engine->engineId()).state, QString(PhosphorEngine::WindowPlacement::stateTiled()));
}

void TestScrollEngineFullscreenFloat::aCommittedSizeReportDuringTheHoldIsIgnored()
{
    // During the hold the window's frame is the output rect, which is not an
    // answer to its column. Taken as one, it set a floor that widened the
    // column and moved its neighbours.
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    ScrollState* state = stateFor(engine);
    QVERIFY(state);
    const ColumnWidth widthBefore = state->strip().columns().at(1).width;
    QVERIFY(engine->setWindowFullscreenFloat(kB, true, kScreen));
    QSignalSpy tiled(engine, &ScrollEngine::windowsTiled);
    engine->reportCommittedSize(kB, QSize(600, 900), QSize(1920, 1080));
    QCoreApplication::processEvents();
    QCOMPARE(state->strip().columns().at(1).width, widthBefore);
    QCOMPARE(tiled.count(), 0);
}

void TestScrollEngineFullscreenFloat::aCloseDuringTheHoldKeepsTheHoldAnswerUntilThePrune()
{
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    ScrollState* state = stateFor(engine);
    QVERIFY(state);
    QVERIFY(engine->setWindowFullscreenFloat(kB, true, kScreen));

    engine->windowClosed(kB);
    QCoreApplication::processEvents();
    QCOMPARE(state->strip().columnCount(), 2);
    QVERIFY(!engine->isWindowTracked(kB));
    // The daemon's close capture runs AFTER Tiling.windowClosed and asks
    // this, so the answer has to outlive the tile.
    QVERIFY(engine->isFullscreenHeld(kB));
    // The compositor's later return has no hold to end.
    QVERIFY(!engine->setWindowFullscreenFloat(kB, false, kScreen));
    QCOMPARE(state->strip().columnCount(), 2);
    // Spent by the capture; reaped on aliveness as the belt.
    engine->forgetClosedFullscreenHold(kB);
    QVERIFY(!engine->isFullscreenHeld(kB));
    QVERIFY(engine->setWindowFullscreenFloat(kA, true, kScreen));
    engine->windowClosed(kA);
    QVERIFY(engine->isFullscreenHeld(kA));
    engine->pruneStaleWindows({kC});
    QCoreApplication::processEvents();
    QVERIFY(!engine->isFullscreenHeld(kA));
    QCOMPARE(state->strip().columnCount(), 1);
}

void TestScrollEngineFullscreenFloat::aReopenOfAClosedHeldWindowConsumesTheHoldAnswer()
{
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    ScrollState* state = stateFor(engine);
    QVERIFY(state);
    QVERIFY(engine->setWindowFullscreenFloat(kB, true, kScreen));
    engine->windowClosed(kB);
    QVERIFY(engine->isFullscreenHeld(kB));
    engine->windowOpened(kB, kScreen, 0, 0);
    QCoreApplication::processEvents();
    QVERIFY(engine->isWindowTracked(kB));
    QVERIFY(!engine->isFullscreenHeld(kB));
    QVERIFY(!state->isFloating(kB));
    QCOMPARE(state->strip().columnCount(), 3);
}

void TestScrollEngineFullscreenFloat::aModeReleaseEndsTheHoldWithoutAFloatAnnouncement()
{
    // The hold is not a float, so a release has no float bit to clear: the
    // window leaves the engine as the tile it is.
    QObject owner;
    ScrollEngine* engine = threeColumns(&owner);
    QVERIFY(engine->setWindowFullscreenFloat(kB, true, kScreen));
    QVERIFY(!engine->isModeSpecificFloated(kB));
    QSignalSpy passive(engine, &PhosphorEngine::PlacementEngineBase::windowFloatingStateSynced);
    QSignalSpy released(engine, &PhosphorEngine::PlacementEngineBase::windowsReleased);

    engine->setActiveScreens({});
    QVERIFY(!engine->isFullscreenHeld(kB));
    QCOMPARE(passive.count(), 0);
    QVERIFY(!released.isEmpty());
    QVERIFY(released.last().at(0).toStringList().contains(kB));
}

QTEST_GUILESS_MAIN(TestScrollEngineFullscreenFloat)
#include "test_scrollengine_fullscreenfloat.moc"
