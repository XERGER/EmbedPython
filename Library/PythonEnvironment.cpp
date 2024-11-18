#include <Python.h>
#include "PythonEnvironment.h"
#include <QJsonObject>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>

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
#ifdef Q_OS_WIN
	HANDLE lockedFileHandle = INVALID_HANDLE_VALUE;
#else
	int lockedFileDescriptor = -1;
#endif
	PyThreadState* mainThreadState;
}

// Constructor
PythonEnvironment::PythonEnvironment()
	: isInitialized(false) {
	mainThreadState = nullptr;
	pythonHome = getDefaultEnvPath();
	pythonPath = getSitePackagesPath();
}

PythonEnvironment::PythonEnvironment(const QString& envPath)
	: isInitialized(false), pythonHome(envPath) {
	pythonPath = getSitePackagesPath();
}

// Destructor
PythonEnvironment::~PythonEnvironment() {
	QMutexLocker locker(&mutex);
	if (Py_IsInitialized()) {
		PyEval_RestoreThread(mainThreadState);
		Py_Finalize();
	}
	locker.unlock();
	unlockPythonExecutable();
}

// Getter functions
QString PythonEnvironment::getPythonExecutablePath() const {
#ifdef Q_OS_WIN
	return QDir(pythonHome).filePath("python.exe");
#else
	return QDir(pythonHome).filePath("bin/python3");
#endif
}

QString PythonEnvironment::getSitePackagesPath() const {
	return QDir(pythonHome).filePath("Lib/site-packages");
}

QString PythonEnvironment::getLibPath() const {
	return QDir(pythonHome).filePath("Lib");
}

QString PythonEnvironment::getDLLsPath() const {
	return QDir(pythonHome).filePath("DLLs");
}

QString PythonEnvironment::getDefaultEnvPath() const {
	QString appDir = QCoreApplication::applicationDirPath();
	QDir pythonDir(appDir);
// 	if (!pythonDir.cd("Python")) {
// 		qCritical() << "\"Python/\" directory does not exist next to the executable.";
// 		return QString();
// 	}
	return pythonDir.absolutePath();
}

// Initialization
bool PythonEnvironment::init() {
	QMutexLocker locker(&mutex);

	// Set environment variables
// 	qputenv("PYTHONHOME", pythonHome.toLocal8Bit());
// 	qputenv("PYTHONPATH", pythonPath.toLocal8Bit());

	if (isInitialized) {
		return true;
	}

	// Initialize Python
	PyConfig config;
	PyConfig_InitPythonConfig(&config);

	if (!pythonHome.isEmpty()) {
		PyStatus status = PyConfig_SetString(&config, &config.home, pythonHome.toStdWString().c_str());
		if (PyStatus_Exception(status)) {
			qCritical() << "Failed to set PYTHONHOME.";
			PyConfig_Clear(&config);
			return false;
		}
	}

	PyStatus init_status = Py_InitializeFromConfig(&config);
	PyConfig_Clear(&config);

	if (PyStatus_Exception(init_status)) {
		qCritical() << "Failed to initialize Python interpreter:" << QString::fromUtf8(init_status.err_msg);
		return false;
	}

	if (!Py_IsInitialized()) {
		qCritical() << "Python interpreter is not initialized.";
		return false;
	}

	// Modify sys.path dynamically
	PyObject* sysModule = PyImport_ImportModule("sys");
	if (!sysModule) {
		qCritical() << "Failed to import sys module.";
		return false;
	}

	PyObject* sysPath = PyObject_GetAttrString(sysModule, "path");
	if (!sysPath || !PyList_Check(sysPath)) {
		qCritical() << "Failed to access sys.path.";
		Py_XDECREF(sysPath);
		Py_DECREF(sysModule);
		return false;
	}

	QStringList paths = { getSitePackagesPath(), getLibPath(), getDLLsPath(), pythonHome };
	for (const QString& path : paths) {
		PyObject* pyPath = PyUnicode_FromString(path.toUtf8().constData());
		if (pyPath) {
			PyList_Append(sysPath, pyPath);
			Py_DECREF(pyPath);
		}
		else {
			qWarning() << "Failed to append path to sys.path:" << path;
		}
	}

	Py_DECREF(sysPath);
	Py_DECREF(sysModule);

	mainThreadState = PyEval_SaveThread();
	isInitialized = true;



	locker.unlock();
	installedPackages = listInstalledPackages();
	locker.relock();

	return true;
}

// Package management
QStringList PythonEnvironment::listInstalledPackages() const {
	QMutexLocker locker(&mutex);
	QString pythonExePath = getPythonExecutablePath();


	// Ensure pip is installed
	QStringList ensurePipArgs{ "-m", "ensurepip" };
	QProcess ensurePipProcess;
	ensurePipProcess.setProgram(pythonExePath);
	ensurePipProcess.setArguments(ensurePipArgs);


	if (!verifyPythonExecutable()) {
		qCritical() << "Python executable verification failed.";
		return {};
	}

	ensurePipProcess.start();
	ensurePipProcess.waitForFinished(10000);


	if (ensurePipProcess.exitCode() != 0) {
		qCritical() << "Failed to ensure pip is installed:" << ensurePipProcess.readAllStandardError();
		return QStringList();
	}

	// List installed packages using pip
	QStringList args{ "-m", "pip", "list", "--format=json", "--no-cache-dir" };
	QProcess process;
	process.setProgram(pythonExePath);
	process.setArguments(args);

	if (!verifyPythonExecutable()) {
		qCritical() << "Python executable verification failed.";
		return {};
	}

	process.start();

	if (!process.waitForFinished(10000)) {
		qCritical() << "Failed to execute pip list:" << process.errorString();
		return QStringList();
	}

	QString stdoutStr = process.readAllStandardOutput();
	QString stderrStr = process.readAllStandardError();

	if (process.exitCode() != 0) {
		qCritical() << "pip list failed:" << stderrStr;
		return QStringList();
	}

	// Parse JSON output
	QJsonDocument doc = QJsonDocument::fromJson(stdoutStr.toUtf8());
	if (doc.isNull()) {
		qCritical() << "Failed to parse pip list output as JSON.";
		return QStringList();
	}

	QJsonArray packagesArray = doc.array();
	QStringList packagesList;
	for (const QJsonValue& pkgVal : packagesArray) {
		QJsonObject pkgObj = pkgVal.toObject();
		QString name = pkgObj["name"].toString();
		packagesList.append(name);
	}

	return packagesList;
}

bool PythonEnvironment::isPackageInstalled(const QString& package) const {
	if (installedPackages.isEmpty()) {
		installedPackages = listInstalledPackages();
	}
	return installedPackages.contains(package, Qt::CaseInsensitive);
}

bool PythonEnvironment::installPackage(const QString& package) {

	if (!init()) {
		return false;
	}

	QMutexLocker locker(&mutex);

	if (isPackageInstalled(package)) {
		qDebug() << "Package" << package << "is already installed.";
		return true;
	}

	QString pythonExePath = getPythonExecutablePath();
	unlockPythonExecutable();

	QStringList args{ "-m", "pip", "install", package, "--no-cache-dir", "--target", pythonPath };
	QProcess process;
	process.setProgram(pythonExePath);
	process.setArguments(args);

	if (!verifyPythonExecutable()) {
		qCritical() << "Python executable verification failed.";
		return false;
	}

	process.start();

	if (!process.waitForFinished(30000)) {
		qCritical() << "Failed to execute pip:" << process.errorString();
		return false;
	}

	lockPythonExecutable();
	bool success = (process.exitCode() == 0);

	if (success) {
		installedPackages.append(package);
		qDebug() << "Package" << package << "installed successfully.";
	}
	else {
		qCritical() << "Failed to install package" << package << ":" << process.readAllStandardError();
	}

	return success;
}

bool PythonEnvironment::lockPythonExecutable() const {


	QString pythonExePath = getPythonExecutablePath();
	if (pythonExePath.isEmpty()) {
		qCritical() << "Python executable path is empty. Cannot lock.";
		return false;
	}
// 
// #ifdef Q_OS_WIN
// 	// Convert QString to wide string
// 	std::wstring widePath = pythonExePath.toStdWString();
// 	lockedFileHandle = CreateFileW(
// 		widePath.c_str(),
// 		GENERIC_READ,
// 		0,
// 		NULL,
// 		OPEN_EXISTING,
// 		FILE_ATTRIBUTE_NORMAL,
// 		NULL
// 	);
// 	if (lockedFileHandle == INVALID_HANDLE_VALUE) {
// 		qCritical() << "Failed to lock Python executable:" << GetLastError();
// 		return false;
// 	}
// #else
// 	// Unix-like systems: Open the file and apply an exclusive lock
// 	lockedFileDescriptor = open(pythonExePath.toStdString().c_str(), O_RDONLY);
// 	if (lockedFileDescriptor == -1) {
// 		qCritical() << "Failed to open Python executable for locking:" << strerror(errno);
// 		return false;
// 	}
// 	if (flock(lockedFileDescriptor, LOCK_EX | LOCK_NB) == -1) {
// 		qCritical() << "Failed to lock Python executable:" << strerror(errno);
// 		close(lockedFileDescriptor);
// 		lockedFileDescriptor = -1;
// 		return false;
// 	}
// #endif
// 	qDebug() << "Python executable locked successfully.";
	return true;
}

bool PythonEnvironment::unlockPythonExecutable() const {

	QString pythonExePath = getPythonExecutablePath();
// 	if (pythonExePath.isEmpty()) {
// 		qCritical() << "Python executable path is empty. Cannot unlock.";
// 		return false;
// 	}
// 
// #ifdef Q_OS_WIN
// 	if (lockedFileHandle != INVALID_HANDLE_VALUE) {
// 		if (!CloseHandle(lockedFileHandle)) {
// 			qCritical() << "Failed to unlock Python executable:" << GetLastError();
// 			return false;
// 		}
// 		lockedFileHandle = INVALID_HANDLE_VALUE;
// 	}
// 	else {
// 		qWarning() << "Python executable was not locked.";
// 	}
// #else
// 	if (lockedFileDescriptor != -1) {
// 		if (flock(lockedFileDescriptor, LOCK_UN) == -1) {
// 			qCritical() << "Failed to unlock Python executable:" << strerror(errno);
// 			// Even if unlocking fails, attempt to close the file descriptor
// 		}
// 		if (close(lockedFileDescriptor) == -1) {
// 			qCritical() << "Failed to close file descriptor:" << strerror(errno);
// 			return false;
// 		}
// 		lockedFileDescriptor = -1;
// 	}
// 	else {
// 		qWarning() << "Python executable was not locked.";
// 	}
// #endif

	return true;
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
