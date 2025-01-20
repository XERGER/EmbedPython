#include "ServerController.h"
#include <QDebug>
#include <QCoreApplication>
#include <QFile>
#include <QCryptographicHash>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>

void forceKillPythonEngineWindows() {
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE) {
		qCritical() << "Failed to create process snapshot.";
		return;
	}
	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hProcessSnap, &pe32)) {
		CloseHandle(hProcessSnap);
		qCritical() << "Failed to retrieve process information.";
		return;
	}
	do {
		if (QString::fromWCharArray(pe32.szExeFile).compare("PythonEngine.exe", Qt::CaseInsensitive) == 0) {
			HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
			if (hProcess == nullptr) {
				qCritical() << "Failed to open process for termination.";
				continue;
			}
			if (!TerminateProcess(hProcess, 0)) {
				qCritical() << "Failed to terminate PythonEngine.exe. Error:" << GetLastError();
			}
			else {
				qDebug() << "PythonEngine.exe terminated successfully.";
			}
			CloseHandle(hProcess);
		}
	} while (Process32Next(hProcessSnap, &pe32));
	CloseHandle(hProcessSnap);
}
#else // Q_OS_UNIX
#include <signal.h>
#include <sys/types.h>
void forceKillPythonEngine() {
	QProcess process;
	process.start("pgrep", QStringList() << "-f" << "PythonEngine");
	process.waitForFinished();
	QString output = process.readAllStandardOutput();
	QRegularExpression re("\\d+"); // Match process IDs
	QRegularExpressionMatchIterator it = re.globalMatch(output);
	while (it.hasNext()) {
		QRegularExpressionMatch match = it.next();
		pid_t pid = match.captured(0).toInt();
		if (kill(pid, SIGKILL) == 0) {
			qDebug() << "Successfully killed PythonEngine process with PID:" << pid;
		}
		else {
			qCritical() << "Failed to kill process with PID:" << pid;
		}
	}
}
#endif
ServerController::ServerController(QString const& enginePath, QObject* parent)
    : QObject(parent), expectedHash(QByteArray::fromHex("79a9490751578b27ca7b7f559c134f464342582c5a03049b63d7ed8fd5ea73dd")) { // Replace with your actual hash
    
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
   // killExistingServers();
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
	// Kill other executables named "PythonEngine"
 #ifdef Q_OS_WIN
 	forceKillPythonEngineWindows();
 #else
 	forceKillPythonEngine();
 #endif

// 	QProcess process;
// 	process.start(command);
// 	process.waitForFinished();
// 
// 	if (process.exitStatus() == QProcess::NormalExit) {
// 		qDebug() << "All existing server instances terminated.";
// 	}
// 	else {
// 		qWarning() << "Failed to terminate existing server instances.";
// 	}
}
void ServerController::restartServer() {
    qDebug() << "Restarting the server process...";
    startServer();
}

bool ServerController::verifyHash(const QString& filePath, const QByteArray& expectedHash) {
	return true;
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
