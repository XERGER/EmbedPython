#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include "global.h"
#ifdef Q_OS_WIN
#include <Windows.h>
#endif

/**
 * @brief The Python class initializes and manages the Python interpreter.
 *        It handles package management and provides thread-safe access to the interpreter.
 */
class LIBRARY_EXPORT PythonEnvironment
{
    
public:


    explicit PythonEnvironment();
    explicit PythonEnvironment(const QString& envPath);
    virtual ~PythonEnvironment();


    QString getPythonExecutablePath() const;

    QString getSitePackagesPath() const;
	QString getLibPath() const;
	QString getDLLsPath() const;

    bool init() const;


    bool isPackageInstalled(const QString& package) const;
    bool installPackage(const QString& package) const;
    QStringList listInstalledPackages() const;
     
    bool installLocalPackage(const QString& packagePath) const;
    bool installOpenAPIClient() const;


    QString getDefaultEnvPath() const;
	QString computeFileHash(const QString& filePath) const;
	bool verifyPythonExecutable() const;
    bool lockPythonExecutable() const;

private:

    mutable QRecursiveMutex mutex;

    QString deobfuscateExpectedHash() const;
    bool unlockPythonExecutable() const;
    mutable bool isInitialized;
    QString pythonHome;
    QString pythonPath;
    mutable std::atomic<int> lockCounter = 0;
    mutable QStringList installedPackages;

};
