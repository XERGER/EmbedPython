#ifndef PYTHONRUNNERIMPL_H
#define PYTHONRUNNERIMPL_H
#include <Python.h>
#include <memory>
#include <atomic>
#include <QFutureWatcher>
#include <QTimer>

#include <QList>
#include "PythonResult.h"

class PythonEnvironment;

class PythonRunner::Impl {
public:
	Impl(std::shared_ptr<PythonEnvironment> const& pythonInstance);
	~Impl();

	PythonResult runScript(const QString& script, const QVariantList& arguments, int timeout);
	QFuture<PythonResult> runScriptAsync(const QString& script, const QVariantList& arguments, int timeout);
	void cancel();
	PythonResult checkSyntax(const QString& script);

private:
	std::shared_ptr<PythonEnvironment> pythonEnv;

	PyObject* sysModule;
	PyObject* ioModule;
	PyObject* stringIOClass;
	PyObject* getValueMethod;

	struct ScriptExecutionContext {
		QFutureWatcher<PythonResult>* watcher;
		QTimer* timeoutTimer;
		std::atomic<bool> isCancelled;
	};

	QList<ScriptExecutionContext*> executions;

	void cleanupExecution(ScriptExecutionContext* context);
};

#endif // PYTHONRUNNERIMPL_H
