#include "ServerController.h"
#include <QDebug>
#include <QCoreApplication>
#include <QFile>
#include <QCryptographicHash>
#include <QFileInfo>

ServerController::ServerController(QString const& enginePath, QObject* parent)
    : QObject(parent), expectedHash(QByteArray::fromHex("2ae44ce55f6dcfb5bbd75d428dfb522ad580cd554990db174bfa31c342338c5e")) { // Replace with your actual hash
    
	if (QFile::exists(enginePath)) {
	
	}
	else {
		qCritical() << "Engine executable not found at:" << enginePath;
	}
   
    connect(&serverProcess, &QProcess::errorOccurred, this, &ServerController::handleServerCrash);
    connect(&serverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ServerController::handleServerCrash);

    restartTimer.setInterval(5000); // 5 seconds interval for restart attempts
    restartTimer.setSingleShot(true);
    connect(&restartTimer, &QTimer::timeout, this, &ServerController::restartServer);

    serverExecutablePath = enginePath;

}

ServerController::~ServerController() {
    stopServer();
}

void ServerController::startServer() {
    if (serverProcess.state() == QProcess::Running) {
        qDebug() << "Server is already running.";
        return;
    }
    killExistingServers();
    // Verify the hash of the executable
    if (!verifyHash(serverExecutablePath, expectedHash)) {
        emit hashMismatch();
        return;
    }

    serverProcess.setProgram(serverExecutablePath);
    serverProcess.setProcessChannelMode(QProcess::MergedChannels);
    serverProcess.start();

    if (serverProcess.waitForStarted(5000)) {
        qDebug() << "Server started successfully.";
        emit serverStarted();
    } else {
        qCritical() << "Failed to start the server process. Error:" << serverProcess.errorString();
    }
}

void ServerController::stopServer() {
	if (serverProcess.state() == QProcess::Running) {
		qDebug() << "Stopping the server process.";

		// Attempt to terminate the process gracefully
		serverProcess.terminate();
		if (!serverProcess.waitForFinished(5000)) { // 10 seconds timeout
			qWarning() << "Terminate failed, killing the server process.";

			// Force kill the process if it doesn't respond
			serverProcess.kill();
			if (!serverProcess.waitForFinished(5000)) { // Additional 5 seconds for forced kill
				qCritical() << "Failed to kill the server process. It may still be running.";
				return;
			}
		}

		qDebug() << "Server process stopped successfully.";
		emit serverStopped();
	}
	else {
		qDebug() << "Server process is not running.";
	}
}

bool ServerController::isServerRunning() const {
    return serverProcess.state() == QProcess::Running;
}

void ServerController::handleServerCrash() {
    qWarning() << "Server process crashed or stopped unexpectedly.";
    emit serverCrashed();

    if (!restartTimer.isActive()) {
        restartTimer.start();
    }
}

void ServerController::killExistingServers() {
	qDebug() << "Checking for existing server processes...";
	QString command;
#if defined(Q_OS_WIN)
	command = QString("taskkill /F /IM %1").arg("PythonEngine.exe");
#elif defined(Q_OS_UNIX)
	command = QString("pkill -f %1").arg("PythonEngine.exe");
#else
	qCritical() << "Unsupported platform for killing processes.";
	return;
#endif

	QProcess process;
	process.start(command);
	process.waitForFinished();

	if (process.exitStatus() == QProcess::NormalExit) {
		qDebug() << "All existing server instances terminated.";
	}
	else {
		qWarning() << "Failed to terminate existing server instances.";
	}
}
void ServerController::restartServer() {
    qDebug() << "Restarting the server process...";
    startServer();
}

bool ServerController::verifyHash(const QString& filePath, const QByteArray& expectedHash) {
   
#if defined(_DEBUG)
	return true; // Skip hash verification in debug builds
#endif

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open file for hash verification:" << file.errorString();
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        qCritical() << "Failed to calculate hash of the file.";
        return false;
    }

    const auto computedHash = hash.result();
    qDebug() << "Computed hash:" << computedHash.toHex();

    return computedHash == expectedHash;
}
