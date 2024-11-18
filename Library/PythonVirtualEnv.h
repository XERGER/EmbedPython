// PythonVirtualEnv.h
#pragma once
#include <QString>
#include <QDir>
#include "global.h"

class LIBRARY_EXPORT PythonVirtualEnv
{
public:
	explicit PythonVirtualEnv(const QString& envPath);
	bool create();
	bool activate();
	QString getEnvPath() const;
	QString getPythonExecutable() const;

private:
	QString envPath;
};
