#include "splashassetresolver.h"

#include <QFile>

QString SplashAssetResolver::resolveSplashAnimationPath()
{
    const QString resourcePath = QStringLiteral(":/assets/aurix_splash.gif");
    if (QFile::exists(resourcePath)) {
        return resourcePath;
    }

    return QString();
}
