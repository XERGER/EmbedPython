#include "PythonRunner.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QProcessEnvironment>
#include <QPointer>

PythonRunner::PythonRunner(QObject* parent)
	: QObject(parent), pythonHome(getDefaultEnvPath()), pythonExecutablePath(getPythonExecutablePath())
{
}

PythonRunner::~PythonRunner() {
	// Clean up any remaining executions
	for (auto data : executions) {
		if (data->process->state() != QProcess::NotRunning) {
			data->process->kill();
		}
		data->process->deleteLater();
		data->timer->deleteLater();
		data->promise.finish();
		delete data;
	}
}

// Getter functions
QString PythonRunner::getPythonExecutablePath() const {
#ifdef Q_OS_WIN
	return QDir(pythonHome).filePath("python.exe");
#else
	return QDir(pythonHome).filePath("bin/python3");
#endif
}

QString PythonRunner::getSitePackagesPath() const {
	QDir pythonEnvDir(getDefaultEnvPath());
	pythonEnvDir.cd("Lib"); // Navigate to the Lib directory
	pythonEnvDir.cd("site-packages"); // Navigate to the site-packages directory
	return pythonEnvDir.absolutePath();
}

QString PythonRunner::getDefaultEnvPath() const {
	QDir pythonDir(QCoreApplication::applicationDirPath());
	pythonDir.cd("python");
	return pythonDir.absolutePath();
}
QFuture<PythonResult> PythonRunner::runScriptAsync(const QString& executionId, const QString& script, const QVariantList& arguments, int timeout) {
	QPromise<PythonResult> promise;
	QFuture<PythonResult> future = promise.future();

	QProcess* process = new QProcess();
	QProcessEnvironment environment;


	environment.insert("PYTHONPATH", getSitePackagesPath());
	environment.insert("PYTHONHOME", getDefaultEnvPath());
	//env.insert("PYTHONUNBUFFERED", "1");


	process->setProgram(pythonExecutablePath); // Adjust as needed
	QStringList procArguments;
	procArguments << "-c" << script;
	process->setArguments(procArguments);
	process->setWorkingDirectory(getDefaultEnvPath());

	process->setProcessEnvironment(environment);

	QElapsedTimer* elapsedTimer = new QElapsedTimer();
	elapsedTimer->start();

	QTimer* timeoutTimer = nullptr;
	if (timeout != -1) {
		timeoutTimer = new QTimer(this);
		timeoutTimer->setSingleShot(true);
		timeoutTimer->setInterval(timeout);
	}

	ExecutionData* data = new ExecutionData{ executionId, process, timeoutTimer, std::move(promise), elapsedTimer };
	executions.insert(executionId, data);


	connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, &PythonRunner::onProcessFinished);
	connect(process, &QProcess::errorOccurred,
		this, &PythonRunner::onProcessErrorOccurred);

	if (timeoutTimer) {
		connect(timeoutTimer, &QTimer::timeout,
			this, &PythonRunner::onTimeout);
	}

	process->start();

	if (timeoutTimer && timeout != -1) {
		timeoutTimer->start();
	}

	return future;
}

void PythonRunner::onTimeout() {
	QTimer* senderTimer = qobject_cast<QTimer*>(sender());
	if (!senderTimer)
		return;

	QString executionId;
	ExecutionData* data = nullptr;

	for (auto it = executions.begin(); it != executions.end(); ++it) {
		if (it.value()->timer == senderTimer) {
			executionId = it.key();
			data = it.value();
			break;
		}
	}

	if (!data) {
		senderTimer->deleteLater();
		return;
	}

	qWarning() << "Timeout occurred for executionId:" << executionId;

	if (data->process->state() != QProcess::NotRunning) {
		data->process->kill();
		data->process->waitForFinished(1000);
	}

	PythonResult timeoutResult(data->executionId, false, "", "Execution timed out.", data->elapsedTimer->elapsed());
	data->promise.addResult(timeoutResult);
	data->promise.finish();

	cleanUpExecutionData(executionId, data);
}

void PythonRunner::cleanUpExecutionData(const QString& executionId, ExecutionData* data) {
	if (data->timer) {
		data->timer->stop();
		data->timer->deleteLater();
	}

	data->process->deleteLater();
	delete data->elapsedTimer;
	delete data;
	executions.remove(executionId);
}

void PythonRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
	QProcess* senderProc = qobject_cast<QProcess*>(sender());
	if (!senderProc)
		return;

	QString executionId;
	ExecutionData* data = nullptr;

	for (auto it = executions.begin(); it != executions.end(); ++it) {
		if (it.value()->process == senderProc) {
			executionId = it.key();
			data = it.value();
			break;
		}
	}

	if (!data) {
		senderProc->deleteLater();
		return;
	}

	if (data->timer) {
		data->timer->stop();
		data->timer->deleteLater();
	}

	QString output = senderProc->readAllStandardOutput();
	QString errorOutput = senderProc->readAllStandardError();
	bool success = (exitStatus == QProcess::NormalExit) && (exitCode == 0);

	PythonResult result(executionId, success, output, errorOutput, data->elapsedTimer->elapsed());
	data->promise.addResult(result);
	data->promise.finish();

	emit scriptFinished(executionId, result);

	cleanUpExecutionData(executionId, data);
}

void PythonRunner::onProcessErrorOccurred(QProcess::ProcessError error) {
	QProcess* senderProc = qobject_cast<QProcess*>(sender());
	if (!senderProc)
		return;

	QString executionId;
	ExecutionData* data = nullptr;
	for (auto it = executions.begin(); it != executions.end(); ++it) {
		if (it.value()->process == senderProc) {
			executionId = it.key();
			data = it.value();
			break;
		}
	}

	if (!data) {
		senderProc->deleteLater();
		return;
	}

	data->timer->stop();
	data->timer->deleteLater();

	QString output = senderProc->readAllStandardOutput();
	QString errorOutput = senderProc->readAllStandardError();
	PythonResult result(data->executionId, false, output, errorOutput + " Process error occurred.", 0);

	data->promise.addResult(result);
	data->promise.finish();

	emit scriptFinished(executionId, result);
	cleanUpExecutionData(executionId, data);
}

bool PythonRunner::cancel(const QString& executionId) {
	if (!executions.contains(executionId)) {
		qWarning() << "Cancel requested for unknown executionId:" << executionId;
		return false;
	}

	ExecutionData* data = executions.value(executionId);

	if (data->process->state() != QProcess::NotRunning) {
		data->process->kill(); // Terminate the process
		data->process->waitForFinished(1000);
	}

	// Set the promise result to indicate cancellation
	PythonResult canceledResult(executionId, false, "", "Execution canceled by user.", 0);
	data->promise.addResult(canceledResult);
	data->promise.finish();

	cleanUpExecutionData(executionId, data);

	return true;
}