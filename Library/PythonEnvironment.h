#pragma once
#include "PythonResult.h"
#include <QMutex>

/**
 * @brief The Python class initializes and manages the Python interpreter.
 *        It handles package management and provides thread-safe access to the interpreter.
 */
class LIBRARY_EXPORT PythonEnvironment : public QObject
{
    Q_OBJECT
public:
    explicit PythonEnvironment();
    explicit PythonEnvironment(const QString& envPath);
    virtual ~PythonEnvironment();

    bool isPackageInstalled(const QString& package) const;
    void installPackage(QString const& executionId, const QString& package) const;
    void reinstallPackage(QString const& executionId, const QString& package) const;
    void updatePackage(QString const& executionId, const QString& package) const;

    void installLocalPackage(QString const& executionId, const QString& packagePath) const;
    void updateLocalPackage(QString const& executionId, const QString& packagePath) const;
    void uninstallPackage(QString const& executionId, const QString& package) const;

    QString getPackageVersion(const QString& package) const;
    QJsonObject getPackageInfo(const QString& package) const;

    void upgradeAllPackages() const;
    QStringList searchPackage(const QString& query) const;
    QStringList listInstalledPackages() const;

signals:
	void packageOperationFinished(QString const& executionId, OperationType operation, const QString& packageName, const PythonResult& result) const;
	void packageOperationProgress(QString const& executionId,  OperationType operation, const QString& packageName, const QString& progressMessage) const;

private:
	bool initEnvironment() const;


	void performPackageOperation(QString const& executionId, OperationType operation, const QString& identifier, const QStringList& args) const;
    QString getSitePackagesPath() const;
	bool verifyPythonExecutable() const;
	bool lockPythonExecutable() const;
	bool unlockPythonExecutable() const;

	QString getDefaultEnvPath() const;
	QString getPythonExecutablePath() const;


    mutable QRecursiveMutex mutex;
    QString computeFileHash(const QString& filePath) const;
    QString deobfuscateExpectedHash() const;

    QString pythonHome;
    QString pythonPath;

    mutable std::atomic<int> lockCounter = 0;
};
