#include "PythonRunnerImpl.h"
#include "PythonEnvironment.h"
#include <QElapsedTimer>
#include <QDebug>
#include <QtConcurrent>
#include <signal.h>
#include "DataConverter.h"


PythonRunner::Impl::Impl(std::shared_ptr<PythonEnvironment> const& pythonInstance)
	: pythonEnv(pythonInstance), sysModule(nullptr), ioModule(nullptr), stringIOClass(nullptr), getValueMethod(nullptr) {
	if (!pythonInstance->init()) {
		qCritical() << "Failed to initialize Python environment.";
	}
	if (!Py_IsInitialized()) {
		Py_Initialize();
	}

	PyGILState_STATE gstate = PyGILState_Ensure();

	sysModule = PyImport_ImportModule("sys");
	ioModule = PyImport_ImportModule("io");
	if (sysModule && ioModule) {
		stringIOClass = PyObject_GetAttrString(ioModule, "StringIO");
		getValueMethod = PyUnicode_FromString("getvalue");
	}
	else {
		qCritical() << "Failed to initialize Python modules.";
	}

	PyGILState_Release(gstate);
}

PythonRunner::Impl::~Impl() {
	PyGILState_STATE gstate = PyGILState_Ensure();
	Py_XDECREF(sysModule);
	Py_XDECREF(ioModule);
	Py_XDECREF(stringIOClass);
	Py_XDECREF(getValueMethod);
	PyGILState_Release(gstate);
}

PythonResult PythonRunner::Impl::runScript(const QString& script, const QVariantList& arguments, int timeout) {
	Q_UNUSED(timeout);
	if (script.isEmpty())
		return PythonResult(false, "", "Script is Empty.");

	QElapsedTimer timer; // Timer to measure execution time
	timer.start();

	// Acquire Python GIL
	PyGILState_STATE gstate = PyGILState_Ensure();

	try {
		// Create new StringIO objects for stdout and stderr
		PyObject* stringIOOut = PyObject_CallObject(stringIOClass, nullptr);
		PyObject* stringIOErr = PyObject_CallObject(stringIOClass, nullptr);
		if (!stringIOOut || !stringIOErr) {
			Py_XDECREF(stringIOOut);
			Py_XDECREF(stringIOErr);
			PyGILState_Release(gstate);
			return PythonResult(false, "", "Failed to create StringIO objects.");
		}

		// Swap sys.stdout and sys.stderr
		PyObject* oldStdout = PyObject_GetAttrString(sysModule, "stdout");
		PyObject* oldStderr = PyObject_GetAttrString(sysModule, "stderr");
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
					// Restore sys.stdout and sys.stderr
					PyObject_SetAttrString(sysModule, "stdout", oldStdout);
					PyObject_SetAttrString(sysModule, "stderr", oldStderr);
					Py_XDECREF(oldStdout);
					Py_XDECREF(oldStderr);
					Py_DECREF(stringIOOut);
					Py_DECREF(stringIOErr);
					PyGILState_Release(gstate);
					return PythonResult(false, "", "Failed to convert argument to PyObject.");
				}
				PyDict_SetItemString(mainDict, varName.toUtf8().constData(), argPy);
				Py_DECREF(argPy); // Decrement reference as PyDict_SetItemString increments it
			}
		}

		PyObject* resultObj = PyRun_String(script.toUtf8().constData(), Py_file_input, mainDict, mainDict);

		// Restore sys.stdout and sys.stderr
		PyObject_SetAttrString(sysModule, "stdout", oldStdout);
		PyObject_SetAttrString(sysModule, "stderr", oldStderr);
		Py_XDECREF(oldStdout);
		Py_XDECREF(oldStderr);

		QString outputStr, errorOutputStr;
		bool success = (resultObj != nullptr);

		if (resultObj) {
			Py_DECREF(resultObj);
		}
		else {
			PyErr_Print();
			errorOutputStr = "Script execution failed.";
		}

		// Retrieve output and error messages
		PyObject* outputObj = PyObject_CallMethodObjArgs(stringIOOut, getValueMethod, nullptr);
		PyObject* errorOutputObj = PyObject_CallMethodObjArgs(stringIOErr, getValueMethod, nullptr);
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

		PyGILState_Release(gstate);

		qint64 elapsedTime = timer.elapsed();
		return PythonResult(success, outputStr, errorOutputStr, QJsonObject(), elapsedTime);
	}
	catch (...) {
		PyGILState_Release(gstate);
		return PythonResult(false, "", "An unknown error occurred.");
	}
	
	
}

QFuture<PythonResult> PythonRunner::Impl::runScriptAsync(const QString& script, const QVariantList& arguments, int timeout) {
	auto context = new ScriptExecutionContext();
	context->isCancelled.store(false);
	context->watcher = new QFutureWatcher<PythonResult>(this);
	context->timeoutTimer = nullptr;

	// Add context to the list
	executions.append(context);

	// Start asynchronous execution using QtConcurrent
	QFuture<PythonResult> future = QtConcurrent::run([this, script, arguments, context]() -> PythonResult {
		if (context->isCancelled.load()) {
			return PythonResult(false, "", "Execution was cancelled.");
		}
		return this->runScript(script, arguments);
		});

	// Set the future to the watcher
	context->watcher->setFuture(future);

	// Implement timeout using QTimer
	if (timeout > 0) {
		context->timeoutTimer = new QTimer(this);
		context->timeoutTimer->setSingleShot(true);
		QObject::connect(context->timeoutTimer, &QTimer::timeout, this, [this, context]() {
			if (!context->watcher->isFinished()) {
				qWarning() << "Script execution timed out. Cancelling...";
				context->isCancelled.store(true);
				PyGILState_STATE gstate = PyGILState_Ensure();
				PyErr_SetInterrupt(); // Interrupt the Python interpreter
				PyGILState_Release(gstate);
			}
			});
		context->timeoutTimer->start(timeout);
	}

	// Connect to the finished signal to clean up
	connect(context->watcher, &QFutureWatcher<PythonResult>::finished, this, [this, context]() {
		// Stop the timer if it's still running
		if (context->timeoutTimer && context->timeoutTimer->isActive()) {
			context->timeoutTimer->stop();
		}
		// Cleanup
		cleanupExecution(context);
		});

	return future;
}

void PythonRunner::Impl::cancel() {
	// Cancel all running executions
	for (auto context : executions) {
		context->isCancelled.store(true);
		PyGILState_STATE gstate = PyGILState_Ensure();
		PyErr_SetInterrupt();
		PyGILState_Release(gstate);
	}
}

PythonResult PythonRunner::Impl::checkSyntax(const QString& script) {
	if (script.isEmpty()) {
		return PythonResult(false, "", "Script is empty.");
	}

	QElapsedTimer timer;
	timer.start();

	// Acquire Python GIL
	PyGILState_STATE gstate = PyGILState_Ensure();

	try {
		// Attempt to compile the script
		PyObject* compiledCode = Py_CompileString(script.toUtf8().constData(), "<string>", Py_file_input);
		if (compiledCode) {
			// Compilation succeeded, syntax is correct
			Py_DECREF(compiledCode);
			PyGILState_Release(gstate);
			qint64 elapsedTime = timer.elapsed();
			return PythonResult(true, "", "", QJsonObject(), elapsedTime);
		}
		else {
			// Compilation failed, retrieve the error message
			PyObject* type, * value, * traceback;
			PyErr_Fetch(&type, &value, &traceback);
			PyErr_NormalizeException(&type, &value, &traceback);
			QString errorMsg = "Syntax error.";
			if (value) {
				PyObject* strExcValue = PyObject_Str(value);
				if (strExcValue) {
					errorMsg = QString::fromUtf8(PyUnicode_AsUTF8(strExcValue));
					Py_DECREF(strExcValue);
				}
			}
			Py_XDECREF(type);
			Py_XDECREF(value);
			Py_XDECREF(traceback);
			PyGILState_Release(gstate);
			qint64 elapsedTime = timer.elapsed();
			return PythonResult(false, "", errorMsg, QJsonObject(), elapsedTime);
		}
	}
	catch (...) {
		PyGILState_Release(gstate);
		return PythonResult(false, "", "An unknown error occurred during syntax checking.");
	}
}

void PythonRunner::Impl::cleanupExecution(ScriptExecutionContext* context) {
	executions.removeAll(context);
	if (context->timeoutTimer) {
		context->timeoutTimer->deleteLater();
	}
	context->watcher->deleteLater();
	delete context;
}
