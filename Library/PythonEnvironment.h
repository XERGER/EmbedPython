#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include "global.h"

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

    bool init();

    bool installPackage(const QString& package);
    bool isPackageInstalled(const QString& package) const;
    QStringList listInstalledPackages() const;

    QMutex* getInterpreterMutex() const;


    QString getDefaultEnvPath() const;
	QString computeFileHash(const QString& filePath) const;
	bool verifyPythonExecutable() const;

private:
    bool lockPythonExecutable() const;


    QString deobfuscateExpectedHash() const;
    bool unlockPythonExecutable() const;
    bool isInitialized;
    QString pythonHome;
    QString pythonPath;
    mutable QMutex mutex;
    mutable QStringList installedPackages;
};
