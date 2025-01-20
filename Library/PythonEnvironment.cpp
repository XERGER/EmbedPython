// PythonEnvironment.cpp


#include "PythonEnvironment.h"
#include <QJsonObject>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QCryptographicHash>
#include <QPointer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDateTime>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#endif

namespace {
	QString operationToString(OperationType operation) {
		switch (operation) {
		case OperationType::Install:
			return "Install";
		case OperationType::Reinstall:
			return "Reinstall";
		case OperationType::Update:
			return "Update";
		case OperationType::InstallLocal:
			return "Install Local";
		case OperationType::UpdateLocal:
			return "Update Local";
		case OperationType::Uninstall:
			return "Uninstall";
		default:
			return "Unknown Operation";
		}
	}


#ifdef Q_OS_WIN
    HANDLE lockedFileHandle = INVALID_HANDLE_VALUE;
#else
    int lockedFileDescriptor = -1;
#endif
 //   PyThreadState* mainThreadState;
}

// Constructor
PythonEnvironment::PythonEnvironment() : PythonEnvironment(getDefaultEnvPath())
{

}

PythonEnvironment::PythonEnvironment(const QString& envPath): pythonHome(envPath), pythonPath(getSitePackagesPath()) {

	initEnvironment();
}

// Destructor
PythonEnvironment::~PythonEnvironment() {
    QMutexLocker locker(&mutex);

    locker.unlock();
    unlockPythonExecutable();
}

QString PythonEnvironment::getSitePackagesPath() const {
	QDir pythonEnvDir(getDefaultEnvPath());
	pythonEnvDir.cd("Lib"); // Navigate to the Lib directory
	pythonEnvDir.cd("site-packages"); // Navigate to the site-packages directory
	return pythonEnvDir.absolutePath();
}

QString PythonEnvironment::getDefaultEnvPath() const {
	QDir pythonDir(QCoreApplication::applicationDirPath());
	pythonDir.cd("python");
	return pythonDir.absolutePath();
}

// Getter functions
QString PythonEnvironment::getPythonExecutablePath() const {
#ifdef Q_OS_WIN
    return QDir(pythonHome).filePath("python.exe");
#else
    return QDir(pythonHome).filePath("bin/python3");
#endif
}

// Initialize Environment (Ensure pip is installed)
bool PythonEnvironment::initEnvironment() const {
    QString pythonExePath = getPythonExecutablePath();
    if (pythonExePath.isEmpty()) {
        qCritical() << "Python executable path is empty.";
        return false;
    }
    // Ensure pip is installed
    QStringList ensurePipArgs{ "-m", "ensurepip" };

	QProcess process;
	QProcessEnvironment environment;


	environment.insert("PYTHONPATH", getSitePackagesPath());
	environment.insert("PYTHONHOME", getDefaultEnvPath());


	process.setArguments(ensurePipArgs);
	process.setProgram(pythonExePath);
	process.setProcessEnvironment(environment);

    if (!verifyPythonExecutable()) {
        qCritical() << "Python executable verification failed.";
        return false;
    }

	process.start();
    if (!process.waitForFinished(30000)) {
        qCritical() << "Failed to ensure pip is installed:" << process.errorString();
        return false;
    }

    if (process.exitCode() != 0) {
        qCritical() << "Failed to ensure pip is installed:" << process.readAllStandardError();
        return false;
    }

	if (!verifyPythonExecutable()) {
		qCritical() << "Python executable verification failed.";
		return false;
	}
    lockPythonExecutable();

    return true;
}

// Package management
bool PythonEnvironment::isPackageInstalled(const QString& package) const {
	QMutexLocker locker(&mutex);
	QStringList args{ "-m", "pip", "show", package };
	QProcess process;
	QProcessEnvironment environment;


	environment.insert("PYTHONPATH", getSitePackagesPath());
	environment.insert("PYTHONHOME", getDefaultEnvPath());


	process.setArguments(args);
	process.setProgram(getPythonExecutablePath());
	process.setProcessEnvironment(environment);

	if (!verifyPythonExecutable()) {
		qCritical() << "Python executable verification failed.";
		return false;
	}

	process.start();
	if (!process.waitForFinished(5000)) {
		qCritical() << "Failed to execute pip show:" << process.errorString();
		return false;
	}

	return process.exitCode() == 0;
}

QStringList PythonEnvironment::listInstalledPackages() const {
	QMutexLocker locker(&mutex); // Ensure thread safety

	QStringList installedPackages;

	// Define the metadata patterns to identify installed packages
	QStringList metadataPatterns;
#ifdef Q_OS_WIN
	// On Windows, package naming conventions might differ if needed
	// Adjust patterns if necessary
	metadataPatterns << "*.egg-info" << "*.dist-info";
#else
	// On Unix-like systems
	metadataPatterns << "*.egg-info" << "*.dist-info";
#endif

	QDir targetDir(pythonPath);
	// Get the list of metadata directories/files matching the patterns
	QStringList metadataFiles = targetDir.entryList(metadataPatterns, QDir::Dirs | QDir::Files, QDir::Name);

	for (const QString& file : metadataFiles) {
		// Extract the package name from the metadata directory/file
		QString packageName;
		if (file.endsWith(".egg-info")) {
			packageName = file.left(file.length() - QString(".egg-info").length());
		}
		else if (file.endsWith(".dist-info")) {
			packageName = file.left(file.length() - QString(".dist-info").length());
		}

		// Handle cases where package names might include version numbers
		// For example: "requests-2.25.1.dist-info" -> "requests"
		int lastDash = packageName.lastIndexOf('-');
		if (lastDash != -1) {
			packageName = packageName.left(lastDash);
		}

		// Avoid duplicates
		if (!installedPackages.contains(packageName, Qt::CaseInsensitive)) {
			installedPackages << packageName;
		}
	}

	qDebug() << "Installed packages:" << installedPackages;

	return installedPackages;
}

QString PythonEnvironment::getPackageVersion(const QString& package) const {
    QMutexLocker locker(&mutex);
    QStringList args{ "-m", "pip", "show", package };
	QProcess process;
	QProcessEnvironment environment;


	environment.insert("PYTHONPATH", getSitePackagesPath());
	environment.insert("PYTHONHOME", getDefaultEnvPath());


	process.setArguments(args);
	process.setProgram(getPythonExecutablePath());
	process.setProcessEnvironment(environment);
	process.setWorkingDirectory(getDefaultEnvPath());

    if (!verifyPythonExecutable()) {
        qCritical() << "Python executable verification failed.";
        return QString();
    }

    process.start();
    if (!process.waitForFinished(5000)) {
        qCritical() << "Failed to execute pip show:" << process.errorString();
        return QString();
    }

    if (process.exitCode() != 0) {
        qCritical() << "pip show failed for package:" << package;
        return QString();
    }

    QString output = process.readAllStandardOutput();
    QString version;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (line.startsWith("Version:")) {
            version = line.section(':', 1).trimmed();
            break;
        }
    }
    return version;
}

QJsonObject PythonEnvironment::getPackageInfo(const QString& package) const {
    QMutexLocker locker(&mutex);
    QStringList args{ "-m", "pip", "show", package };

	QProcess process;
	QProcessEnvironment environment;


	environment.insert("PYTHONPATH", getSitePackagesPath());
	environment.insert("PYTHONHOME", getDefaultEnvPath());


	process.setArguments(args);
	process.setProgram(getPythonExecutablePath());
	process.setProcessEnvironment(environment);
	process.setWorkingDirectory(getDefaultEnvPath());

    if (!verifyPythonExecutable()) {
        qCritical() << "Python executable verification failed.";
        return QJsonObject();
    }

    process.start();
    if (!process.waitForFinished(5000)) {
        qCritical() << "Failed to execute pip show:" << process.errorString();
        return QJsonObject();
    }

    if (process.exitCode() != 0) {
        qCritical() << "pip show failed for package:" << package;
        return QJsonObject();
    }

    QString output = process.readAllStandardOutput();
    QJsonObject pkgInfo;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        int sepIndex = line.indexOf(':');
        if (sepIndex != -1) {
            QString key = line.left(sepIndex).trimmed();
            QString value = line.mid(sepIndex + 1).trimmed();
            pkgInfo.insert(key, value);
        }
    }
    return pkgInfo;
}


void PythonEnvironment::performPackageOperation(QString const& executionId, OperationType operation, const QString& identifier, const QStringList& args) const {
	QMutexLocker locker(&mutex);
	QString packageName = identifier;

	if (operation == OperationType::InstallLocal || operation == OperationType::UpdateLocal) {
		packageName = QDir(identifier).dirName();

		// Check if the specified path exists
		if (!QFileInfo::exists(identifier) || !QFileInfo(identifier).isReadable()) {
			const QString error = QString("Local package path '%1' does not exist or is not accessible.").arg(identifier);
			qCritical() << error;
			emit packageOperationFinished(executionId, operation, packageName, PythonResult(executionId, false, QString(), error));
			return;
		}

		// Check if the package is already installed for local operations
		if (isPackageInstalled(packageName)) {
			const QString message = QString("Local package '%1' is already installed.").arg(packageName);
			qDebug() << message;
			emit packageOperationFinished(executionId, operation, packageName, PythonResult(executionId, true, message, QString()));
			return;
		}
	}

	// Check preconditions
	if ((operation == OperationType::Install || operation == OperationType::Reinstall || operation == OperationType::Update) &&
		operation != OperationType::InstallLocal && operation != OperationType::UpdateLocal) {
		if (operation == OperationType::Install && isPackageInstalled(packageName)) {
			const QString message = QString("Package '%1' is already installed.").arg(packageName);
			qDebug() << message;
			emit packageOperationFinished(executionId, operation, packageName, PythonResult(executionId, true, message, QString()));
			return;
		}
		if (operation == OperationType::Update && !isPackageInstalled(packageName)) {
			const QString message = QString("Package '%1' is not installed. Cannot update.").arg(packageName);
			qDebug() << message;
			emit packageOperationFinished(executionId, operation, packageName, PythonResult(executionId, false, QString(), message));
			return;
		}
	}

	if (!verifyPythonExecutable()) {
		const QString error = "Python executable verification failed.";
		qCritical() << error;
		emit packageOperationFinished(executionId, operation, packageName, PythonResult(executionId, false, QString(), error));
		return;
	}

	QProcess* process = new QProcess();
	QProcessEnvironment environment;


	environment.insert("PYTHONPATH", getSitePackagesPath());
	environment.insert("PYTHONHOME", getDefaultEnvPath());
	environment.insert("PATH", getDefaultEnvPath() + ";" + getPythonExecutablePath());
	//env.insert("PYTHONUNBUFFERED", "1");
	process->setArguments(args);
	process->setProgram(getPythonExecutablePath());
	//process->setProcessEnvironment(environment);
	process->setWorkingDirectory(getDefaultEnvPath());

	qDebug() << getPythonExecutablePath() << args.join(" ");
	qDebug() << "Environment Variables:";
	foreach(const QString & key, environment.keys()) {
		qDebug() << key << "=" << environment.value(key);
	}

	// Connect process signals
	connect(process, &QProcess::readyReadStandardOutput, this, [this, executionId, process, operation, identifier]() {
		const QString output = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
		if (output.startsWith("Collecting")) {
			emit packageOperationProgress(executionId, operation, identifier, "Collecting package information...");
		}
		else if (output.startsWith("Downloading")) {
			emit packageOperationProgress(executionId, operation, identifier, "Downloading package...");
		}
		else if (output.startsWith("Installing")) {
			emit packageOperationProgress(executionId, operation, identifier, "Installing package...");
		}
		else if (!output.isEmpty()) {
			emit packageOperationProgress(executionId, operation, identifier, output);
		}
		});

	connect(process, &QProcess::readyReadStandardError, this, [this, executionId, process, operation, identifier]() {
		const QString errorOutput = QString::fromUtf8(process->readAllStandardError()).trimmed();
		if (!errorOutput.isEmpty()) {
			emit packageOperationProgress(executionId, operation, identifier, errorOutput);
		}
		});

	connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		[this, executionId, process, operation, identifier, packageName](int exitCode, QProcess::ExitStatus) {
			const QString stdoutStr = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
			const QString stderrStr = QString::fromUtf8(process->readAllStandardError()).trimmed();
			const qint64 endTime = QDateTime::currentMSecsSinceEpoch();
			if (exitCode != 0) {
				const QString error = QString("Operation '%1' failed for package '%2': %3")
					.arg(operationToString(operation), packageName, stderrStr);
				qCritical() << error;
				emit packageOperationFinished(executionId, operation, identifier, PythonResult(executionId, false, stdoutStr, error, endTime));
			}
			else {
				const QString message = QString("Operation '%1' succeeded for package '%2'.")
					.arg(operationToString(operation), packageName);
				qDebug() << message;
				emit packageOperationFinished(executionId, operation, identifier, PythonResult(executionId, true, stdoutStr, QString(), endTime));
			}
			process->deleteLater();
		});

	// Start the process
	qDebug() << "Starting package operation...";
	process->start();
}


void PythonEnvironment::installPackage(QString const& executionId, const QString& package) const {
    QStringList args{ "-m", "pip", "install", package, "--no-cache-dir", "--target", pythonPath };
    performPackageOperation(executionId, OperationType::Install, package, args);
}

void PythonEnvironment::reinstallPackage(QString const& executionId, const QString& package) const {
    QStringList args{ "-m", "pip", "install", "--force-reinstall", package, "--no-cache-dir", "--target", pythonPath };
    performPackageOperation(executionId, OperationType::Reinstall, package, args);
}

void PythonEnvironment::updatePackage(QString const& executionId, const QString& package) const {
    QStringList args{ "-m", "pip", "install", "--upgrade", package, "--no-cache-dir", "--target", pythonPath };
    performPackageOperation(executionId, OperationType::Update, package, args);
}

void PythonEnvironment::installLocalPackage(QString const& executionId, const QString& packagePath) const {
    QStringList args{
        "-m",
        "pip",
        "install",
        packagePath,
        "--no-cache-dir",   // Do not use cache to ensure fresh installation
        "--target", pythonPath
    };

    performPackageOperation(executionId, OperationType::InstallLocal, packagePath, args);
}

void PythonEnvironment::updateLocalPackage(QString const& executionId, const QString& packagePath) const {
    QStringList args{
        "-m",
        "pip",
        "install",
        "--upgrade",
        packagePath,
        "--no-cache-dir",
        "--target",
        pythonPath
    };
    performPackageOperation(executionId, OperationType::UpdateLocal, packagePath, args);
}

void PythonEnvironment::uninstallPackage(QString const& executionId, const QString& package) const {
	QMutexLocker locker(&mutex); // Ensure thread safety

	// Construct the package directory and metadata paths
	QString packageDirName = package;
#ifdef Q_OS_WIN
	// On Windows, packages may have different naming conventions
	// Adjust if necessary
#else
	// On Unix-like systems, ensure correct casing if needed
#endif
	QString packagePath = QDir(pythonPath).filePath(packageDirName);

	// Function to delete directories and files
	auto removePath = [](const QString& path) {
		QDir dir(path);
		if (dir.exists()) {
			bool success = dir.removeRecursively(); // Deletes the directory and its contents
			if (!success) {
				qWarning() << "Failed to remove directory:" << path;
			}
		}
		else {
			QFile file(path);
			if (file.exists()) {
				bool success = file.remove(); // Deletes the file
				if (!success) {
					qWarning() << "Failed to remove file:" << path;
				}
			}
		}
		};

	// Remove the package directory
	removePath(packagePath);

	// Remove related metadata files (e.g., .egg-info, .dist-info)
	QDir targetDir(pythonPath);
	QStringList metadataPatterns;
#ifdef Q_OS_WIN
	metadataPatterns << QString("%1-*.egg-info").arg(package)
		<< QString("%1-*.dist-info").arg(package);
#else
	metadataPatterns << QString("%1-*.egg-info").arg(package)
		<< QString("%1-*.dist-info").arg(package);
#endif

	QStringList metadataFiles = targetDir.entryList(metadataPatterns, QDir::Files | QDir::Dirs, QDir::Name);
	for (const QString& file : metadataFiles) {
		QString fullPath = targetDir.filePath(file);
		removePath(fullPath);
	}

	qDebug() << "Uninstalled package:" << package;
	emit packageOperationFinished(executionId, OperationType::Uninstall, package, PythonResult(executionId, true, QString("Uninstalled package: ") + package, QString(), QDateTime::currentMSecsSinceEpoch()));
}


// Additional Package Information Methods


void PythonEnvironment::upgradeAllPackages() const {
// 	QMutexLocker locker(&mutex);
// 	QStringList installed = listInstalledPackages();
// 	for (const QString& package : installed) {
// 		QStringList args{ "-m", "pip", "install", "--upgrade", package, "--no-cache-dir" };
// 		performPackageOperation(OperationType::Update, package, args);
// 	}
}

QStringList PythonEnvironment::searchPackage(const QString& query) const {
	QMutexLocker locker(&mutex);
	QStringList args{ "-m", "pip", "search", query };
	QProcess process;
	process.setProgram(getPythonExecutablePath());
	process.setArguments(args);

	if (!verifyPythonExecutable()) {
		qCritical() << "Python executable verification failed.";
		return QStringList();
	}

	process.start();
	if (!process.waitForFinished(5000)) {
		qCritical() << "Failed to execute pip search:" << process.errorString();
		return QStringList();
	}

	if (process.exitCode() != 0) {
		qCritical() << "pip search failed for query:" << query;
		return QStringList();
	}

	QString output = process.readAllStandardOutput();
	QStringList results;
	QStringList lines = output.split('\n', Qt::SkipEmptyParts);
	for (const QString& line : lines) {
		QString packageName = line.section(' ', 0, 0).trimmed();
		results.append(packageName);
	}
	return results;
}


// Verification and hashing
QString PythonEnvironment::computeFileHash(const QString& filePath) const {
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly)) {
		qCritical() << "Failed to open file for hashing:" << filePath;
		return QString();
	}
	QCryptographicHash hash(QCryptographicHash::Sha256);
	if (!hash.addData(&file)) {
		qCritical() << "Failed to compute hash for file:" << filePath;
		return QString();
	}
	return hash.result().toHex();
}

bool PythonEnvironment::verifyPythonExecutable() const {
	return true;

	QString pythonExePath = getPythonExecutablePath();
	QString computedHash = computeFileHash(pythonExePath);
	if (computedHash.isEmpty()) {
		qCritical() << "Failed to compute hash for Python executable.";
		return false;
	}
	QString deobfuscatedHash = deobfuscateExpectedHash();
	if (computedHash != deobfuscatedHash) {
		qCritical() << "Python executable hash mismatch! Possible tampering detected.";
		return false;
	}
	return true;
}

QString PythonEnvironment::deobfuscateExpectedHash() const {
	const QString obfuscatedHash = "8cc674d3e003f07032a3eb367d8b5c213bb7352bb6ba0bc69f65e142fc64127e";
	const QString salt = "s0m3S@ltV@lu3";
	QByteArray obfHashBytes = QByteArray::fromHex(obfuscatedHash.toUtf8());
	QByteArray saltBytes = salt.toUtf8();
	QByteArray deobfHashBytes;
	for (int i = 0; i < obfHashBytes.size(); ++i) {
		deobfHashBytes.append(obfHashBytes[i] ^ saltBytes[i % saltBytes.size()]);
	}
	return QString(deobfHashBytes.toHex());
}

bool PythonEnvironment::lockPythonExecutable() const {
	QMutexLocker locker(&mutex); // Thread safety
	if (lockCounter.fetch_add(1, std::memory_order_relaxed) == 0) {
#ifdef Q_OS_WIN
		QString pythonExePath = getPythonExecutablePath();
		if (pythonExePath.isEmpty()) {
			qCritical() << "Python executable path is empty. Cannot lock.";
			lockCounter.fetch_sub(1, std::memory_order_relaxed);
			return false;
		}
		std::wstring widePath = pythonExePath.toStdWString();
		lockedFileHandle = CreateFileW(
			widePath.c_str(),
			FILE_GENERIC_EXECUTE,       // Request execute permissions
			FILE_SHARE_READ,            // Allow shared read access
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);
		if (lockedFileHandle == INVALID_HANDLE_VALUE) {
			qCritical() << "Failed to lock Python executable:" << GetLastError();
			lockCounter.fetch_sub(1, std::memory_order_relaxed);
			return false;
		}
#else
		QString pythonExePath = getPythonExecutablePath();
		if (pythonExePath.isEmpty()) {
			qCritical() << "Python executable path is empty. Cannot lock.";
			lockCounter.fetch_sub(1, std::memory_order_relaxed);
			return false;
		}
		lockedFileDescriptor = open(pythonExePath.toStdString().c_str(), O_RDONLY);
		if (lockedFileDescriptor == -1) {
			qCritical() << "Failed to open Python executable for locking:" << strerror(errno);
			lockCounter.fetch_sub(1, std::memory_order_relaxed);
			return false;
		}
		if (flock(lockedFileDescriptor, LOCK_SH | LOCK_NB) == -1) { // Shared lock
			qCritical() << "Failed to lock Python executable:" << strerror(errno);
			close(lockedFileDescriptor);
			lockedFileDescriptor = -1;
			lockCounter.fetch_sub(1, std::memory_order_relaxed);
			return false;
		}
#endif
	}
	return true;
}

bool PythonEnvironment::unlockPythonExecutable() const {
	QString pythonExePath = getPythonExecutablePath();
	if (pythonExePath.isEmpty()) {
		qCritical() << "Python executable path is empty. Cannot unlock.";
		return false;
	}
#ifdef Q_OS_WIN
	if (lockedFileHandle != INVALID_HANDLE_VALUE) {
		if (!CloseHandle(lockedFileHandle)) {
			qCritical() << "Failed to unlock Python executable:" << GetLastError();
			return false;
		}
		lockedFileHandle = INVALID_HANDLE_VALUE;
	}
	else {
		qWarning() << "Python executable was not locked.";
	}
#else
	if (lockedFileDescriptor != -1) {
		if (flock(lockedFileDescriptor, LOCK_UN) == -1) {
			qCritical() << "Failed to unlock Python executable:" << strerror(errno);
			// Even if unlocking fails, attempt to close the file descriptor
		}
		if (close(lockedFileDescriptor) == -1) {
			qCritical() << "Failed to close file descriptor:" << strerror(errno);
			return false;
		}
		lockedFileDescriptor = -1;
	}
	else {
		qWarning() << "Python executable was not locked.";
	}
#endif
	return true;
}