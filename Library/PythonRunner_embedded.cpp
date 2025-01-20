#include <Python.h>
#include "PythonRunner.h"
#include <QElapsedTimer>
#include <QDebug>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QTimer>
#include <atomic>
#include <QList>
#include "DataConverter.h"
#include "PythonEnvironment.h"

// Definition of the Impl class inside PythonRunner.cpp
class PythonRunner::Impl {
public:
	Impl(QObject* parent);
	~Impl();

	PythonResult runScript(const QString& script, const QVariantList& arguments, int timeout);
	void cancel();
	PythonResult checkSyntax(const QString& script);

	struct ScriptExecutionContext {
		QString executionId;
		QFutureWatcher<PythonResult>* watcher;
		QTimer* timeoutTimer;
		std::atomic<bool> isCancelled;
	};

	QString getDefaultEnvPath() const;

	QString getSitePackagesPath() const;
	QString getLibPath() const;
	QString getDLLsPath() const;

	QMap<QString, ScriptExecutionContext*> executions;
	QMutex executionsMutex;


	// In PythonRunner::Impl
	void cancel(const QString& executionId);

private:
	QObject* parentObject; // Store parent QObject

	PyObject* sysModule;
	PyObject* ioModule;
	PyObject* stringIOClass;
	PyObject* getValueMethod;
};

// Constructor
PythonRunner::Impl::Impl(QObject* parent)
	:  parentObject(parent),
	sysModule(nullptr), ioModule(nullptr), stringIOClass(nullptr), getValueMethod(nullptr) {

	if (!Py_IsInitialized()) {
		Py_Initialize();
	}

	PyGILState_STATE gstate = PyGILState_Ensure();

	sysModule = PyImport_ImportModule("sys");
	ioModule = PyImport_ImportModule("io");
	if (sysModule && ioModule) {

		stringIOClass = PyObject_GetAttrString(ioModule, "StringIO");
		getValueMethod = PyUnicode_FromString("getvalue");


		if (!stringIOClass || !getValueMethod)
		{
			qCritical() << "Failed to intialize io";
		}
	}
	else {
		qCritical() << "Failed to initialize Python modules.";
	}
	PyObject* sysPath = PyObject_GetAttrString(sysModule, "path");
	if (!sysPath || !PyList_Check(sysPath)) {
		qCritical() << "Failed to access sys.path.";
		Py_XDECREF(sysPath);
		Py_DECREF(sysModule);
	}
	QStringList paths = { getSitePackagesPath(), getLibPath(), getDLLsPath(), getDefaultEnvPath() };
	for (const QString& path : paths) {
		PyObject* pyPath = PyUnicode_FromString(path.toUtf8().constData());
		if (pyPath) {
			PyList_Append(sysPath, pyPath);
			Py_DECREF(pyPath);
		}
		else {
			qWarning() << "Failed to append path to sys.path:" << path;
		}
	}
	Py_DECREF(sysPath);
	Py_DECREF(sysModule);

	PyGILState_Release(gstate);
}

// Destructor
PythonRunner::Impl::~Impl() {
	PyGILState_STATE gstate = PyGILState_Ensure();
	Py_XDECREF(sysModule);
	Py_XDECREF(ioModule);
	Py_XDECREF(stringIOClass);
	Py_XDECREF(getValueMethod);
	PyGILState_Release(gstate);
}

// Implement runScript
PythonResult PythonRunner::Impl::runScript(const QString& script, const QVariantList& arguments, int timeout) {
	Q_UNUSED(timeout);
	if (script.isEmpty()) {
		return PythonResult(false, "", "Script is Empty.");
	}

	QElapsedTimer timer;
	timer.start();

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

		PyObject_SetAttrString(sysModule, "stdout", stringIOOut);
		PyObject_SetAttrString(sysModule, "stderr", stringIOErr);

		PyObject* mainModule = PyImport_AddModule("__main__");
		PyObject* mainDict = PyModule_GetDict(mainModule);

		if (!arguments.isEmpty()) {
			for (int i = 0; i < arguments.size(); ++i) {
				QString varName = QString("arg%1").arg(i + 1);
				PyObject* argPy = DataConverter::QVariantToPyObject(arguments[i]);
				if (!argPy) {
					Py_DECREF(stringIOOut);
					Py_DECREF(stringIOErr);
					PyGILState_Release(gstate);
					return PythonResult(false, "", "Failed to convert argument to PyObject.");
				}
				PyDict_SetItemString(mainDict, varName.toUtf8().constData(), argPy);
				Py_DECREF(argPy);
			}
		}

		PyObject* resultObj = PyRun_String(script.toUtf8().constData(), Py_file_input, mainDict, mainDict);


		QString outputStr, errorOutputStr;
		bool success = (resultObj != nullptr);

		if (resultObj) {
			Py_DECREF(resultObj);
		}
		else {
			PyErr_Print();
			errorOutputStr = "Script execution failed.";
		}

		PyObject* outputObj = PyObject_CallMethodObjArgs(stringIOOut, getValueMethod, NULL);
		PyObject* errorOutputObj = PyObject_CallMethodObjArgs(stringIOErr, getValueMethod, NULL);

		if (outputObj) {
			outputStr = QString::fromUtf8(PyUnicode_AsUTF8(outputObj));
			Py_DECREF(outputObj);
		}
		else {
			qDebug() << "Failed to capture output.";
			PyErr_Print();
			errorOutputStr += "Failed to capture output.";
		}

		if (errorOutputObj) {
		
			errorOutputStr += QString::fromUtf8(PyUnicode_AsUTF8(errorOutputObj));
			Py_DECREF(errorOutputObj);
		}
		else {
			qDebug() << "Failed to capture error output.";
			PyErr_Print();
			errorOutputStr += "Failed to capture output.";
		}


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


// Implement cancel
void PythonRunner::Impl::cancel() {
	for (auto context : executions) {
		context->isCancelled.store(true);
		PyGILState_STATE gstate = PyGILState_Ensure();
		PyErr_SetInterrupt();
		PyGILState_Release(gstate);
	}
}

// Implement checkSyntax
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

// PythonRunner constructor and destructor
PythonRunner::PythonRunner( QObject* parent)
	: QObject(parent), impl(std::make_unique<Impl>( this)) {}

PythonRunner::~PythonRunner() = default;

// Forward public methods to the Impl
PythonResult PythonRunner::runScript(const QString& script, const QVariantList& arguments, int timeout) {
	return impl->runScript(script, arguments, timeout);
}


// In PythonRunner::Impl
void PythonRunner::Impl::cancel(const QString& executionId) {
	QMutexLocker locker(&executionsMutex);
	if (executionId.isEmpty()) {
		// Cancel all scripts
		for (auto context : executions) {
			context->isCancelled.store(true);
			PyGILState_STATE gstate = PyGILState_Ensure();
			PyErr_SetInterrupt();
			PyGILState_Release(gstate);
		}
	}
	else {
		// Cancel specific script
		auto context = executions.value(executionId, nullptr);
		if (context) {
			context->isCancelled.store(true);
			PyGILState_STATE gstate = PyGILState_Ensure();
			PyErr_SetInterrupt();
			PyGILState_Release(gstate);
		}
		else {
			qWarning() << "No execution found with ID:" << executionId;
		}
	}
}

QFuture<PythonResult> PythonRunner::runScriptAsync(const QString& executionId, const QString& script, const QVariantList& arguments, int timeout) {
	auto context = new Impl::ScriptExecutionContext();
	context->executionId = executionId;
	context->isCancelled.store(false);
	context->watcher = new QFutureWatcher<PythonResult>(this);
	context->timeoutTimer = nullptr;

	{
		QMutexLocker locker(&impl->executionsMutex);
		impl->executions.insert(executionId, context);
	}

	// Start asynchronous execution
	QFuture<PythonResult> future = QtConcurrent::run([this, script, arguments, context]() -> PythonResult {
		if (context->isCancelled.load()) {
			return PythonResult(false, "", "Execution was cancelled.");
		}
		return this->runScript(script, arguments);
		});

	// Set the future to the watcher

	context->watcher->setFuture(future);

	if (timeout > 0) {
		context->timeoutTimer = new QTimer(this);
		context->timeoutTimer->setSingleShot(true);
		connect(context->timeoutTimer, &QTimer::timeout, this, [this, context]() {
			if (!context->watcher->isFinished()) {
				qWarning() << "Script execution timed out. Cancelling execution ID:" << context->executionId;
				this->cancel(context->executionId);
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
		{
			QMutexLocker locker(&impl->executionsMutex);
			impl->executions.remove(context->executionId);
		}
		if (context->timeoutTimer) {
			context->timeoutTimer->deleteLater();
		}
		context->watcher->deleteLater();
		delete context;
	});

	return future;
}


void PythonRunner::cancel() {
	impl->cancel();
}

void PythonRunner::cancel(const QString& executionId)
{
	impl->cancel(executionId);
}

PythonResult PythonRunner::checkSyntax(const QString& script) {
	return impl->checkSyntax(script);
}

QString PythonRunner::Impl::getSitePackagesPath() const {
	return QDir(getDefaultEnvPath()).filePath("Lib/site-packages");
}

QString PythonRunner::Impl::getLibPath() const {
	return QDir(getDefaultEnvPath()).filePath("Lib");
}

QString PythonRunner::Impl::getDLLsPath() const {
	return QDir(getDefaultEnvPath()).filePath("DLLs");
}

QString PythonRunner::Impl::getDefaultEnvPath() const {
	QString appDir = QCoreApplication::applicationDirPath();
	QDir pythonDir(appDir);
	// if (!pythonDir.cd("Python")) {
	//     qCritical() << "\"Python/\" directory does not exist next to the executable.";
	//     return QString();
	// }
	return pythonDir.absolutePath();
}

