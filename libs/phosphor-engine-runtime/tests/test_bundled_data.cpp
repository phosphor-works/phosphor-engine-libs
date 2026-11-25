// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

// The data this tree ships in data/ and the runtime loads for every host: each
// layout and scrolling template satisfies its schema and parses, and each Luau
// algorithm loads into a registry. A broken file would otherwise surface only
// at runtime, as a layout, template or algorithm the host silently skips.

#include <PhosphorFsLoader/SchemaValidator.h>
#include <PhosphorTiles/AlgorithmRegistry.h>
#include <PhosphorTiles/ScriptedAlgorithmLoader.h>
#include <PhosphorZones/Layout.h>
#include <PhosphorZones/ScrollingTemplate.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <optional>

namespace {

QDir dataDir(const QString& sub)
{
    return QDir(QStringLiteral(PHOSPHOR_ENGINE_DATA_DIR) + QLatin1Char('/') + sub);
}

QStringList filesIn(const QString& sub, const QString& pattern)
{
    return dataDir(sub).entryList({pattern}, QDir::Files, QDir::Name);
}

std::optional<QJsonObject> readObject(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot open");
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = parseError.error != QJsonParseError::NoError ? parseError.errorString()
                                                              : QStringLiteral("not a JSON object");
        return std::nullopt;
    }
    return doc.object();
}

PhosphorFsLoader::SchemaValidator schema(const QString& name)
{
    QFile file(dataDir(QStringLiteral("schemas")).filePath(name));
    if (!file.open(QIODevice::ReadOnly)) {
        return PhosphorFsLoader::SchemaValidator(QByteArray());
    }
    return PhosphorFsLoader::SchemaValidator(file.readAll());
}

QString describe(const QList<PhosphorFsLoader::SchemaValidator::Error>& errors)
{
    QStringList parts;
    for (const auto& error : errors) {
        parts.append((error.path.isEmpty() ? QStringLiteral("(root)") : error.path) + QLatin1String(": ")
                     + error.message);
    }
    return parts.join(QLatin1String("; "));
}

} // namespace

class TestBundledData : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void layoutsMatchTheSchemaAndParse()
    {
        const QStringList files = filesIn(QStringLiteral("layouts"), QStringLiteral("*.json"));
        QVERIFY2(!files.isEmpty(), "no bundled layouts");
        const auto validator = schema(QStringLiteral("layout.schema.json"));
        QVERIFY2(validator.isValid(), "layout.schema.json does not compile");

        QStringList failures;
        for (const QString& name : files) {
            QString error;
            const auto object = readObject(dataDir(QStringLiteral("layouts")).filePath(name), &error);
            if (!object) {
                failures.append(name + QLatin1String(": ") + error);
                continue;
            }
            if (const auto errors = validator.validate(*object)) {
                failures.append(name + QLatin1String(": ") + describe(*errors));
                continue;
            }
            std::unique_ptr<PhosphorZones::Layout> layout(PhosphorZones::Layout::fromJson(*object));
            if (!layout || layout->zoneCount() == 0) {
                failures.append(name + QLatin1String(": passes the schema but parses to no zones"));
            }
        }
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
    }

    void scrollingTemplatesMatchTheSchemaAndParse()
    {
        const QStringList files = filesIn(QStringLiteral("scrolling-templates"), QStringLiteral("*.json"));
        QVERIFY2(!files.isEmpty(), "no bundled scrolling templates");
        const auto validator = schema(QStringLiteral("scrolling-template.schema.json"));
        QVERIFY2(validator.isValid(), "scrolling-template.schema.json does not compile");

        QStringList failures;
        for (const QString& name : files) {
            QString error;
            const auto object = readObject(dataDir(QStringLiteral("scrolling-templates")).filePath(name), &error);
            if (!object) {
                failures.append(name + QLatin1String(": ") + error);
                continue;
            }
            if (const auto errors = validator.validate(*object)) {
                failures.append(name + QLatin1String(": ") + describe(*errors));
                continue;
            }
            if (!PhosphorZones::ScrollingTemplate::fromJson(*object).isValid()) {
                failures.append(name + QLatin1String(": passes the schema but fromJson rejects it"));
            }
        }
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
    }

    void everyAlgorithmLoads()
    {
        const QStringList files = filesIn(QStringLiteral("algorithms"), QStringLiteral("*.luau"));
        QVERIFY2(!files.isEmpty(), "no bundled algorithms");

        // The loader reads <XDG data dir>/<subdirectory>, so the bundled tree
        // is linked into a data dir of its own.
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("phosphor-works"))));
        QVERIFY(QFile::link(dataDir(QStringLiteral("algorithms")).absolutePath(),
                            root.filePath(QStringLiteral("phosphor-works/algorithms"))));
        qputenv("XDG_DATA_DIRS", root.path().toUtf8());
        qputenv("XDG_DATA_HOME", root.filePath(QStringLiteral("home")).toUtf8());

        PhosphorTiles::AlgorithmRegistry registry;
        PhosphorTiles::ScriptedAlgorithmLoader loader(QStringLiteral("phosphor-works/algorithms"), &registry);
        loader.scanAndRegister(PhosphorFsLoader::LiveReload::Off);

        QStringList missing;
        for (const QString& name : files) {
            const QString id = QFileInfo(name).completeBaseName();
            if (!registry.algorithm(id)) {
                missing.append(id);
            }
        }
        QVERIFY2(missing.isEmpty(), qPrintable(QLatin1String("not loaded: ") + missing.join(QLatin1String(", "))));
    }
};

QTEST_GUILESS_MAIN(TestBundledData)
#include "test_bundled_data.moc"
