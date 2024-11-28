// PythonRunner.h
#ifndef PYTHONRUNNER_H
#define PYTHONRUNNER_H

#include <QObject>
#include <QFuture>
#include <QFutureWatcher>
#include <QTimer>
#include <memory>
#include <atomic>
#include "PythonResult.h"


class PythonEnvironment;

class LIBRARY_EXPORT PythonRunner : public QObject
{
	Q_OBJECT
public:
	explicit PythonRunner(std::shared_ptr<PythonEnvironment> const& pythonInstance, QObject* parent = nullptr);
	~PythonRunner();

	PythonResult checkSyntax(const QString& script);
	PythonResult runScript(const QString& script, const QVariantList& arguments = {}, int timeout = 0);
	
	QFuture<PythonResult> runScriptAsync(const QString& script, const QVariantList& arguments = {}, int timeout = 0);

	void cancel(); // Modify to cancel all running scripts if necessary

private:
	std::shared_ptr<PythonEnvironment> pythonEnv;

	class Impl;
	std::unique_ptr<Impl> impl; // Pimpl
};

#endif // PYTHONRUNNER_H
