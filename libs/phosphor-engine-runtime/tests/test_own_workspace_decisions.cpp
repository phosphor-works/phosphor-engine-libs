// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

// The pure decisions of the own-workspace feature (PlasmaZones#1201).

#include <QTest>

#include "../src/runtime/controllers/ownworkspacedecisions.h"

using namespace PhosphorEngineRuntime::OwnWorkspaceDecisions;

class TestOwnWorkspaceDecisions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // Every combination: each option covers only its own state, and
    // fullscreen wins when both hold.
    void triggerFor_table_data()
    {
        QTest::addColumn<bool>("fullscreenOption");
        QTest::addColumn<bool>("maximizedOption");
        QTest::addColumn<bool>("fullscreen");
        QTest::addColumn<bool>("maximized");
        QTest::addColumn<int>("expected");
        const int none = static_cast<int>(Trigger::None);
        const int fs = static_cast<int>(Trigger::Fullscreen);
        const int max = static_cast<int>(Trigger::Maximized);
        for (int bits = 0; bits < 16; ++bits) {
            const bool fo = bits & 1;
            const bool mo = bits & 2;
            const bool f = bits & 4;
            const bool m = bits & 8;
            const int expected = (fo && f) ? fs : (mo && m) ? max : none;
            QTest::addRow("fo=%d mo=%d f=%d m=%d", fo, mo, f, m) << fo << mo << f << m << expected;
        }
    }
    void triggerFor_table()
    {
        QFETCH(bool, fullscreenOption);
        QFETCH(bool, maximizedOption);
        QFETCH(bool, fullscreen);
        QFETCH(bool, maximized);
        QFETCH(int, expected);
        QCOMPARE(static_cast<int>(triggerFor(fullscreenOption, maximizedOption, fullscreen, maximized)), expected);
    }

    // The good case enters, and each fact on its own refuses.
    void mayEnter_eachFactRefuses()
    {
        EnterFacts good;
        good.singleDesktop = true;
        good.originShown = true;
        good.othersOnOrigin = 1;
        good.trailingEmpty = true;
        QVERIFY(mayEnter(good));

        EnterFacts f = good;
        f.sticky = true;
        QVERIFY(!mayEnter(f));
        f = good;
        f.singleDesktop = false;
        QVERIFY(!mayEnter(f));
        f = good;
        f.originShown = false;
        QVERIFY(!mayEnter(f));
        f = good;
        f.originIsOwnWorkspace = true;
        QVERIFY(!mayEnter(f));
        f = good;
        f.othersOnOrigin = 0; // already alone
        QVERIFY(!mayEnter(f));
        f = good;
        f.trailingEmpty = false; // the desktop cap
        QVERIFY(!mayEnter(f));
        f = good;
        f.movePending = true;
        QVERIFY(!mayEnter(f));
    }

    void leaveActionFor_table()
    {
        // The origin is gone: nothing to go back to, whatever else holds.
        QCOMPARE(leaveActionFor(false, true, false), LeaveAction::Forget);
        QCOMPARE(leaveActionFor(true, true, false), LeaveAction::Forget);
        // A close shows the origin again.
        QCOMPARE(leaveActionFor(true, true, true), LeaveAction::ReturnScreen);
        QCOMPARE(leaveActionFor(true, false, true), LeaveAction::ReturnScreen);
        // The state ended on its own workspace: the window goes back.
        QCOMPARE(leaveActionFor(false, true, true), LeaveAction::ReturnWindow);
        // The user moved it on: the workspace is an ordinary one now.
        QCOMPARE(leaveActionFor(false, false, true), LeaveAction::Forget);
    }
};

QTEST_GUILESS_MAIN(TestOwnWorkspaceDecisions)
#include "test_own_workspace_decisions.moc"
