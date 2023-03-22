#pragma once
#include "global.h"
#include <QFile>
#include <QVariant>

class LIBRARY_EXPORT Python
{
public:
	Python() = default;

	static std::shared_ptr<Python> createPythonForOs()
	{
		return nullptr;
	}

	virtual bool createPythonEnvironmentOnSystem() const = 0;
	virtual QVariant installPackage(QString const& package) const = 0;
	virtual QVariant runScript(QString const& script, QVariantList const& arguments = {}) const = 0;
	virtual QVariant runScript(QString const& script, QString const& method, QVariantList const& arguments = {}) const = 0;
	virtual QVariant runScript(QFile const& script, QString const& method, QVariantList const& arguments = {}) const = 0;
	virtual QVariant addQObjectToPython(QString const& name, QObject* object) const = 0;
	virtual bool init() const = 0;
};