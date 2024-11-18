#include <Python.h>
#include "PythonRunner.h"

#include <QDebug>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QMutex>
#include <signal.h>
#include "DataConverter.h"

// Define the implementation class
class PythonRunner::Impl {
public:
	Impl(std::shared_ptr<PythonEnvironment> pythonInstance, PythonRunner* runner)
		: pythonEnv(pythonInstance),
		owner(runner),
		futureWatcher(new QFutureWatcher<PythonResult>(runner)),
		isCancelled(false),
		globals(nullptr),
		timeoutTimer(nullptr)
	{}

	~Impl() {
		// Clean up Python globals
// 		if (globals) {
// 			Py_DECREF(globals);
// 		}
	}

	// Core members
	std::shared_ptr<PythonEnvironment> pythonEnv;
	PythonRunner* owner;
	QFutureWatcher<PythonResult>* futureWatcher;
	std::atomic<bool> isCancelled;
	PyObject* globals;

	// Fluent API members
	QString currentScript;
	QVariantList currentArguments;
	int currentTimeout = 0;
	QTimer* timeoutTimer;
	// Instance-specific output captures
	QString instanceCapturedStdout;
	QString instanceCapturedStderr;

	// Mutex for thread safety
	QMutex outputMutex;

	void initPythonEnvironment() {
		if (!pythonEnv->init()) {
			qCritical() << "Failed to initialize Python environment.";
			return;
		}

		// Initialize the Python globals dictionary
		PyGILState_STATE gstate = PyGILState_Ensure();
		globals = PyDict_New();
		if (!globals) {
			qWarning() << "Failed to create globals dictionary.";
		}
		else {
			PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());
		}

		PyGILState_Release(gstate);
	}
};

PythonRunner::PythonRunner(std::shared_ptr<PythonEnvironment> pythonInstance, QObject* parent)
	: QObject(parent), impl(std::make_unique<Impl>(pythonInstance, this)) {

 	impl->initPythonEnvironment();

}

PythonRunner::~PythonRunner() = default;

PythonResult PythonRunner::runScript(const QString& script, const QVariantList& arguments, int timeout) {
	Q_UNUSED(timeout);

	if(script.isEmpty()) return PythonResult(false, "", "Script is Empty.");

	// Ensure Python is initialized
	if (!Py_IsInitialized()) {
		return PythonResult(false, "", "Python interpreter is not initialized.");
	}
	QElapsedTimer timer; // Timer to measure execution time
	timer.start();
	// Acquire Python GIL
	PyGILState_STATE gstate = PyGILState_Ensure();

	try {
		PyObject* sysModule = PyImport_ImportModule("sys");
		PyObject* ioModule = PyImport_ImportModule("io");
		PyObject* stringIOOut = PyObject_CallMethod(ioModule, "StringIO", nullptr);
		PyObject* stringIOErr = PyObject_CallMethod(ioModule, "StringIO", nullptr);

		if (!sysModule || !ioModule || !stringIOOut || !stringIOErr) {
			Py_XDECREF(sysModule);
			Py_XDECREF(ioModule);
			Py_XDECREF(stringIOOut);
			Py_XDECREF(stringIOErr);
			PyGILState_Release(gstate);
			return PythonResult(false, "", "Failed to redirect Python I/O.");
		}

		PyObject_SetAttrString(sysModule, "stdout", stringIOOut);
		PyObject_SetAttrString(sysModule, "stderr", stringIOErr);

		// Execute the script
		PyObject* mainModule = PyImport_AddModule("__main__");
		PyObject* mainDict = PyModule_GetDict(mainModule);

		if (!arguments.isEmpty()) {
			for (int i = 0; i < arguments.size(); ++i) {
				QString varName = QString("arg%1").arg(i + 1); // Variable names: arg1, arg2, etc.
				PyObject* argPy = DataConverter::QVariantToPyObject(arguments[i]);
				if (!argPy) {
					Py_DECREF(stringIOOut);
					Py_DECREF(stringIOErr);
					Py_DECREF(ioModule);
					Py_DECREF(sysModule);
					PyGILState_Release(gstate);
					return PythonResult(false, "", "Failed to convert argument to PyObject.");
				}
				PyDict_SetItemString(mainDict, varName.toUtf8().constData(), argPy);
				Py_DECREF(argPy); // Decrement reference as PyDict_SetItemString increments it
			}
		}

		PyObject* resultObj = PyRun_String(script.toUtf8().constData(), Py_file_input, mainDict, mainDict);

		QString outputStr, errorOutputStr;
		bool success = false;
		QJsonObject returnValue;

		if (resultObj) {
			success = true;

			// Check for a 'result' variable in __main__
			PyObject* resultVar = PyDict_GetItemString(mainDict, "result");
			if (resultVar) {
				QJsonValue retValue = DataConverter::PyObjectToJson(resultVar);
				if (retValue.isObject()) {
					returnValue = retValue.toObject();
				}
				else {
					// Wrap non-object results
					returnValue["result"] = retValue;
				}
			}
			Py_DECREF(resultObj);
		}
		else {
			PyErr_Print();
			errorOutputStr = "Script execution failed.";
		}

		// Retrieve output and error messages
		PyObject* outputObj = PyObject_CallMethod(stringIOOut, "getvalue", nullptr);
		PyObject* errorOutputObj = PyObject_CallMethod(stringIOErr, "getvalue", nullptr);

		if (outputObj) {
			outputStr = QString::fromUtf8(PyUnicode_AsUTF8(outputObj));
			Py_DECREF(outputObj);
		}

		if (errorOutputObj) {
			errorOutputStr += QString::fromUtf8(PyUnicode_AsUTF8(errorOutputObj));
			Py_DECREF(errorOutputObj);
		}

		// Cleanup
		Py_DECREF(stringIOOut);
		Py_DECREF(stringIOErr);
		Py_DECREF(ioModule);
		Py_DECREF(sysModule);

		PyGILState_Release(gstate);

		qint64 elapsedTime = timer.elapsed();

		return PythonResult(success, outputStr, errorOutputStr, returnValue, elapsedTime);

	}
	catch (...) {
		PyGILState_Release(gstate);
		return PythonResult(false, "", "An unknown error occurred.");
	}
}

QFuture<PythonResult> PythonRunner::runScriptAsync(const QString& script, const QVariantList& arguments, int timeout) {
	// Prevent multiple asynchronous executions simultaneously
	if (impl->futureWatcher->isRunning()) {
		qWarning() << "Another script is already running asynchronously.";
		return QFuture<PythonResult>(); // Return an invalid QFuture
	}

	// Reset cancellation flag
	impl->isCancelled.store(false);

	// Start asynchronous execution using QtConcurrent
	QFuture<PythonResult> future = QtConcurrent::run([this, script, arguments, timeout]() -> PythonResult {
		return this->runScript(script, arguments, timeout);
		});

	// Set the future to the watcher
	impl->futureWatcher->setFuture(future);

	// Implement timeout using QTimer
	if (timeout > 0) {
		if (!impl->timeoutTimer) {
			impl->timeoutTimer = new QTimer(this);
			impl->timeoutTimer->setSingleShot(true);
			connect(impl->timeoutTimer, &QTimer::timeout, this, [this]() {
				if (impl->futureWatcher->isRunning()) {
					qWarning() << "Script execution timed out. Cancelling...";
					this->cancel();
				}
				});
		}
		else {
			impl->timeoutTimer->stop();
		}
		impl->timeoutTimer->start(timeout);
	}

	// Connect to the finished signal to stop the timeout timer
	connect(impl->futureWatcher, &QFutureWatcher<PythonResult>::finished, this, [this]() {
		if (impl->timeoutTimer && impl->timeoutTimer->isActive()) {
			impl->timeoutTimer->stop();
		}
		});

	return future;
}

void PythonRunner::cancel() {
	impl->isCancelled.store(true);
	PyGILState_STATE gstate = PyGILState_Ensure();
	PyErr_SetInterrupt();
	PyGILState_Release(gstate);
}

// Fluent API
PythonRunner& PythonRunner::setScript(const QString& script) {
	impl->currentScript = script;
	return *this;
}

PythonRunner& PythonRunner::setArguments(const QVariantList& arguments) {
	impl->currentArguments = arguments;
	return *this;
}

PythonRunner& PythonRunner::setTimeout(int timeout) {
	impl->currentTimeout = timeout;
	return *this;
}

PythonResult PythonRunner::execute() {
	return runScript(impl->currentScript, impl->currentArguments, impl->currentTimeout);
}

PythonRunner& PythonRunner::operator()(const QString& script) {
	return setScript(script);
}

PythonRunner& PythonRunner::operator()(const QString& script, const QVariantList& arguments) {
	return setScript(script).setArguments(arguments);
}

PythonRunner& PythonRunner::operator()(const QString& script, const QVariantList& arguments, int timeout) {
	return setScript(script).setArguments(arguments).setTimeout(timeout);
}
