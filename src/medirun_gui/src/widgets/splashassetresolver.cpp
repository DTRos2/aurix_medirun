#include "splashassetresolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

QString SplashAssetResolver::resolveSplashAnimationPath()
{
    const QString resourcePath = QStringLiteral(":/assets/aurix_splash.gif");
    if (QFile::exists(resourcePath)) {
        return resourcePath;
    }

    const QString currentDir = QDir::currentPath();
    const QString applicationDir = QCoreApplication::applicationDirPath();
    const QString relativeAssetPath = QStringLiteral("assets/aurix_splash.gif");
    const QStringList candidatePaths = {
        QDir(currentDir).filePath(relativeAssetPath),
        QDir(currentDir).filePath(QStringLiteral("../") + relativeAssetPath),
        QDir(currentDir).filePath(QStringLiteral("../../") + relativeAssetPath),
        QDir(applicationDir).filePath(relativeAssetPath),
        QDir(applicationDir).filePath(QStringLiteral("../") + relativeAssetPath),
        QDir(applicationDir).filePath(QStringLiteral("../../") + relativeAssetPath),
        QDir(applicationDir).filePath(QStringLiteral("../../../") + relativeAssetPath),
        QDir(applicationDir).filePath(QStringLiteral("../../../../") + relativeAssetPath)
    };

    for (const QString &candidatePath : candidatePaths) {
        if (QFile::exists(candidatePath)) {
            return candidatePath;
        }
    }

    return QString();
}
