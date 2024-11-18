// PythonVirtualEnv.cpp
#include "PythonVirtualEnv.h"
#include <QProcess>
#include <QDebug>
#include <QFileInfo>

PythonVirtualEnv::PythonVirtualEnv(const QString& envPath)
	: envPath(envPath)
{
}

bool PythonVirtualEnv::create()
{
	// Use the embedded Python to create a virtual environment
	QString pythonExe = getPythonExecutable();
	if (pythonExe.isEmpty())
	{
		qCritical() << "Python executable not found for virtual environment creation.";
		return false;
	}

	QStringList args = { "-m", "venv", envPath };

	QProcess process;
	process.start(pythonExe, args);
	if (!process.waitForFinished())
	{
		qCritical() << "Failed to create virtual environment:" << process.errorString();
		return false;
	}

	if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
	{
		qCritical() << "Virtual environment creation failed:" << process.readAllStandardError();
		return false;
	}

	qDebug() << "Virtual environment created at:" << envPath;
	return true;
}

bool PythonVirtualEnv::activate()
{
	// Activation is handled via environment variables; no action needed here.
	return true;
}

QString PythonVirtualEnv::getEnvPath() const
{
	return envPath;
}

QString PythonVirtualEnv::getPythonExecutable() const
{
#ifdef Q_OS_WIN
	return QDir(envPath).filePath("Scripts/python.exe");
#else
	return QDir(envPath).filePath("bin/python");
#endif
}
