/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "qmlprojectrunconfigurationfactory.h"
#include "qmlprojectmanagerconstants.h"
#include "qmlproject.h"
#include "qmlprojectrunconfiguration.h"

#include <projectexplorer/kitinformation.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/target.h>
#include <qtsupport/qtkitinformation.h>

namespace QmlProjectManager {
namespace Internal {

const char QML_SCENE_SUFFIX[] = ".QmlScene";

QmlProjectRunConfigurationFactory::QmlProjectRunConfigurationFactory(QObject *parent) :
    ProjectExplorer::IRunConfigurationFactory(parent)
{
    setObjectName("QmlProjectRunConfigurationFactory");
    registerRunConfiguration<QmlProjectRunConfiguration>(Constants::QML_RC_ID);
    setSupportedProjectType<QmlProject>();
    setSupportedTargetDeviceTypes({ProjectExplorer::Constants::DESKTOP_DEVICE_TYPE});
}

QList<QString> QmlProjectRunConfigurationFactory::availableBuildTargets(ProjectExplorer::Target *parent, CreationMode) const
{
    QtSupport::BaseQtVersion *version
            = QtSupport::QtKitInformation::qtVersion(parent->kit());

    return (version && version->qtVersion() >= QtSupport::QtVersionNumber(5, 0, 0))
            ? QList<QString>({QML_SCENE_SUFFIX}) : QList<QString>();
}

QString QmlProjectRunConfigurationFactory::displayNameForBuildTarget(const QString &buildTarget) const
{
    QTC_ASSERT(buildTarget == QML_SCENE_SUFFIX, return QString());
    return tr("QML Scene");
}

bool QmlProjectRunConfigurationFactory::canCreateHelper(ProjectExplorer::Target *parent,
                                                        const QString &buildTarget) const
{
    QTC_ASSERT(buildTarget == QML_SCENE_SUFFIX, return false);

    const QtSupport::BaseQtVersion *version
            = QtSupport::QtKitInformation::qtVersion(parent->kit());
    return version && version->qtVersion() >= QtSupport::QtVersionNumber(5, 0, 0);
}

} // namespace Internal
} // namespace QmlProjectManager

