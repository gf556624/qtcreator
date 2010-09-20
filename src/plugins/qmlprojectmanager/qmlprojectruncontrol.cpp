/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "qmlprojectruncontrol.h"
#include "qmlprojectrunconfiguration.h"
#include "qmlprojectconstants.h"
#include <coreplugin/icore.h>
#include <coreplugin/modemanager.h>
#include <projectexplorer/environment.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/applicationlauncher.h>
#include <utils/qtcassert.h>

#include <debugger/debuggerrunner.h>
#include <debugger/debuggerplugin.h>
#include <debugger/debuggerconstants.h>
#include <debugger/debuggeruiswitcher.h>
#include <debugger/debuggerengine.h>
#include <qmljsinspector/qmljsinspectorconstants.h>

#include <QDir>
#include <QLabel>

using ProjectExplorer::RunConfiguration;
using ProjectExplorer::RunControl;

namespace QmlProjectManager {
namespace Internal {

QmlRunControl::QmlRunControl(QmlProjectRunConfiguration *runConfiguration, QString mode)
    : RunControl(runConfiguration, mode)
{
    ProjectExplorer::Environment environment = ProjectExplorer::Environment::systemEnvironment();

    m_applicationLauncher.setEnvironment(environment.toStringList());
    m_applicationLauncher.setWorkingDirectory(runConfiguration->workingDirectory());

    m_executable = runConfiguration->viewerPath();
    m_commandLineArguments = runConfiguration->viewerArguments();

    connect(&m_applicationLauncher, SIGNAL(appendMessage(QString,bool)),
            this, SLOT(slotError(QString, bool)));
    connect(&m_applicationLauncher, SIGNAL(appendOutput(QString, bool)),
            this, SLOT(slotAddToOutputWindow(QString, bool)));
    connect(&m_applicationLauncher, SIGNAL(processExited(int)),
            this, SLOT(processExited(int)));
    connect(&m_applicationLauncher, SIGNAL(bringToForegroundRequested(qint64)),
            this, SLOT(slotBringApplicationToForeground(qint64)));
}

QmlRunControl::~QmlRunControl()
{
}

void QmlRunControl::start()
{
    m_applicationLauncher.start(ProjectExplorer::ApplicationLauncher::Gui, m_executable,
                                m_commandLineArguments);

    emit started();
    emit appendMessage(this, tr("Starting %1 %2").arg(QDir::toNativeSeparators(m_executable),
                                                      m_commandLineArguments.join(QLatin1String(" "))), false);
}

RunControl::StopResult QmlRunControl::stop()
{
    m_applicationLauncher.stop();
    return StoppedSynchronously;
}

bool QmlRunControl::isRunning() const
{
    return m_applicationLauncher.isRunning();
}

void QmlRunControl::slotBringApplicationToForeground(qint64 pid)
{
    bringApplicationToForeground(pid);
}

void QmlRunControl::slotError(const QString &err, bool isError)
{
    emit appendMessage(this, err, isError);
    emit finished();
}

void QmlRunControl::slotAddToOutputWindow(const QString &line, bool onStdErr)
{
    emit addToOutputWindowInline(this, line, onStdErr);
}

void QmlRunControl::processExited(int exitCode)
{
    emit appendMessage(this, tr("%1 exited with code %2").arg(QDir::toNativeSeparators(m_executable)).arg(exitCode), exitCode != 0);
    emit finished();
}

QmlRunControlFactory::QmlRunControlFactory(QObject *parent)
    : IRunControlFactory(parent)
{
}

QmlRunControlFactory::~QmlRunControlFactory()
{
}

bool QmlRunControlFactory::canRun(RunConfiguration *runConfiguration,
                                  const QString &mode) const
{
    QmlProjectRunConfiguration *config = qobject_cast<QmlProjectRunConfiguration*>(runConfiguration);
    if (mode == ProjectExplorer::Constants::RUNMODE) {
        return config != 0;
    } else if (mode == ProjectExplorer::Constants::DEBUGMODE) {
        bool qmlDebugSupportInstalled = Debugger::DebuggerUISwitcher::instance()->supportedLanguages()
                                        & Debugger::QmlLanguage;
        return (config != 0) && qmlDebugSupportInstalled;
    }

    return false;
}

RunControl *QmlRunControlFactory::create(RunConfiguration *runConfiguration,
                                         const QString &mode)
{
    QTC_ASSERT(canRun(runConfiguration, mode), return 0);

    QmlProjectRunConfiguration *config = qobject_cast<QmlProjectRunConfiguration *>(runConfiguration);
    RunControl *runControl = 0;
    if (mode == ProjectExplorer::Constants::RUNMODE) {
       runControl = new QmlRunControl(config, mode);
    } else {
        runControl = createDebugRunControl(config);
    }
    return runControl;
}

QString QmlRunControlFactory::displayName() const
{
    return tr("Run");
}

QWidget *QmlRunControlFactory::createConfigurationWidget(RunConfiguration *runConfiguration)
{
    Q_UNUSED(runConfiguration)
    return new QLabel("TODO add Configuration widget");
}

ProjectExplorer::RunControl *QmlRunControlFactory::createDebugRunControl(QmlProjectRunConfiguration *runConfig)
{
    ProjectExplorer::Environment environment = ProjectExplorer::Environment::systemEnvironment();
    Debugger::DebuggerStartParameters params;
    params.startMode = Debugger::StartInternal;
    params.executable = runConfig->viewerPath();
    params.qmlServerAddress = "127.0.0.1";
    params.qmlServerPort = runConfig->qmlDebugServerPort();
    params.processArgs = runConfig->viewerArguments();
    params.processArgs.append(QLatin1String("-qmljsdebugger=port:") + QString::number(runConfig->qmlDebugServerPort()));
    params.workingDirectory = runConfig->workingDirectory();
    params.environment = environment.toStringList();
    params.displayName = runConfig->displayName();

    Debugger::DebuggerRunControl *debuggerRunControl = Debugger::DebuggerPlugin::createDebugger(params, runConfig);
    return debuggerRunControl;
}

} // namespace Internal
} // namespace QmlProjectManager

