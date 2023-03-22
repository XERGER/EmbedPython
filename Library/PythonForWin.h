#pragma once
#include "global.h"
#include "Python.h"

class PythonForWin : public Python
{
	using Python::Python;

	virtual bool createPythonEnvironmentOnSystem() const
	{
		return true;
	}

	virtual QVariant installPackage(QString const& package) const
	{
		return QVariant();
	}

	virtual QVariant runScript(QString const& script, QVariantList const& arguments) const
	{
		return QVariant();
	}

	virtual QVariant runScript(QFile const& script, QVariantList const& arguments) const
	{
		return QVariant();
	}

	virtual QVariant addQObjectToPython(QObject* object) const
	{
		return QVariant();
	}

	virtual bool init() const
	{
		return true;
	}
};