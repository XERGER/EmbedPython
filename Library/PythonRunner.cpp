#include "PythonRunner.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QProcessEnvironment>
#include <QPointer>

PythonRunner::PythonRunner(QObject* parent)
	: QObject(parent), pythonHome(getDefaultEnvPath()), pythonExecutablePath(getPythonExecutablePath()), environment(createEnviornment())
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


QProcessEnvironment PythonRunner::createEnviornment() const {
	QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

	// 2. Remove Python vars
	environment.remove("PYTHONHOME");
	environment.remove("PYTHONPATH");

#if defined(Q_OS_WIN)
	static const QChar PATH_SEP(';');
#else
	static const QChar PATH_SEP(':');
#endif

	// 3. Filter out any Python directories from PATH
	const auto oldPath = environment.value("PATH");
	QStringList pathParts = oldPath.split(PATH_SEP, Qt::SkipEmptyParts);
	for (int i = pathParts.size() - 1; i >= 0; --i) {
		if (pathParts[i].contains("python", Qt::CaseInsensitive)) {
			pathParts.removeAt(i);
		}
	}

	// 4. Build new PATH, prepending your Python bin/Scripts
#ifdef Q_OS_WIN
	// Example: pythonHome might be "C:/MyApp/python"
	const QString pythonBinPath = QDir(pythonHome).filePath("Scripts");
#else
	// Example: pythonHome might be "/opt/MyApp/python"
	const QString pythonBinPath = QDir(pythonHome).filePath("bin");
#endif
	const QString newPath = pythonBinPath + PATH_SEP + pathParts.join(PATH_SEP);
	environment.insert("PATH", newPath);

	// 5. Insert your Python environment
	environment.insert("PYTHONHOME", pythonHome);
	environment.insert("PYTHONPATH", getSitePackagesPath());
	QString homePath = QDir::homePath();

	// 2. Insert environment variables (instead of setting them in the script)
	environment.insert("HOME", homePath);
	environment.insert("MPLCONFIGDIR", homePath);
	environment.insert("USERPROFILE", homePath);
	environment.insert("PYTHONUTF8", "1");

	return environment;
}



QFuture<PythonResult> PythonRunner::runScriptAsync(const QString& executionId,
	const QString& script,
	const QVariantList& arguments,
	int timeout)
{
	QPromise<PythonResult> promise;
	QFuture<PythonResult> future = promise.future();

	// 6. Configure QProcess
	QProcess* process = new QProcess(this);
	process->setProcessEnvironment(environment);
	process->setProgram(pythonExecutablePath);
	process->setWorkingDirectory(pythonHome); // optional
	//process->setProcessChannelMode(QProcess::MergedChannels);

	QStringList procArgs;
	procArgs << "-u" << "-c" << script;
	// Optional script arguments:
	// for (const auto &arg : arguments) {
	//     procArgs << arg.toString();
	// }
	process->setArguments(procArgs);

	// 7. Optional timeout
	QTimer* timeoutTimer = nullptr;
	if (timeout != -1) {
		timeoutTimer = new QTimer(this);
		timeoutTimer->setSingleShot(true);
		timeoutTimer->setInterval(timeout);
	}

	QElapsedTimer* elapsedTimer = new QElapsedTimer();
	elapsedTimer->start();

	auto* data = new ExecutionData{
		executionId, process, timeoutTimer, std::move(promise), elapsedTimer
	};
	executions.insert(executionId, data);

	// 	// Connect the new output slot
	connect(process, &QProcess::readyReadStandardOutput,
		this, &PythonRunner::onProcessReadyReadStandardOutput);

	// 8. Connect signals
	connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, &PythonRunner::onProcessFinished);

	connect(process, &QProcess::errorOccurred,
		this, &PythonRunner::onProcessErrorOccurred);



	if (timeoutTimer) {
		connect(timeoutTimer, &QTimer::timeout, this, &PythonRunner::onTimeout);
	}

	// 9. Start
	process->start();

	if (timeoutTimer) timeoutTimer->start();
	

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

PythonRunner::ExecutionData* PythonRunner::getExecutionDataFromProcess(QProcess* process) const
{
	ExecutionData* data = nullptr;

	for (auto it = executions.begin(); it != executions.end() && !data; ++it) 
		if (it.value()->process == process) data = it.value();
		
	return data;
}

void PythonRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
	const auto senderProc = qobject_cast<QProcess*>(sender());
	if (!senderProc)
		return;

	const auto data = getExecutionDataFromProcess(senderProc);

	if (!data) {
		senderProc->deleteLater();
		return;
	}

	const auto executionId = data->executionId;

	if (data->timer) {
		data->timer->stop();
		data->timer->deleteLater();
	}

	const auto output = senderProc->readAllStandardOutput();
	const auto errorOutput = senderProc->readAllStandardError();
	bool success = (exitStatus == QProcess::NormalExit) && (exitCode == 0);

	PythonResult result(executionId, success, output, errorOutput, data->elapsedTimer->elapsed());
	data->promise.addResult(result);
	data->promise.finish();

	emit scriptFinished(executionId, result);

	cleanUpExecutionData(executionId, data);
}

void PythonRunner::onProcessErrorOccurred(QProcess::ProcessError error) {

	const auto senderProc = qobject_cast<QProcess*>(sender());
	
	if (!senderProc) return;

	const auto data = getExecutionDataFromProcess(senderProc);

	if (!data) {
		senderProc->deleteLater();
		return;
	}

	const auto executionId = data->executionId;

	data->timer->stop();
	data->timer->deleteLater();

	const auto output = senderProc->readAllStandardOutput();
	const auto errorOutput = senderProc->readAllStandardError();
	PythonResult result(data->executionId, false, output, errorOutput + " Process error occurred.", data->elapsedTimer->elapsed());

	data->promise.addResult(result);
	data->promise.finish();


	cleanUpExecutionData(executionId, data);
}

bool PythonRunner::cancel(const QString& executionId) {
	if (!executions.contains(executionId)) {
		qWarning() << "Cancel requested for unknown executionId:" << executionId;
		return false;
	}

	ExecutionData* data = executions.value(executionId);


	if (!data) return false;

	executions.remove(executionId);

	const auto processToKill = data->process;


	if (processToKill->state() != QProcess::NotRunning) {
		processToKill->kill(); // Terminate the process
	}

	if (data->timer) {
		data->timer->stop();
		data->timer->deleteLater();
	}
	
	const auto output = processToKill->readAllStandardOutput();
	processToKill->deleteLater();

	// Set the promise result to indicate cancellation

	PythonResult canceledResult(executionId, false, output, "Execution canceled by user.", data->elapsedTimer->elapsed());
	data->promise.addResult(canceledResult);
	data->promise.finish();

	delete data->elapsedTimer;
	delete data;


	emit scriptFinished(executionId, canceledResult);

	return true;
}

void PythonRunner::onProcessReadyReadStandardOutput()
{
	QProcess* proc = qobject_cast<QProcess*>(sender());
	if (!proc) return;

	ExecutionData* data = getExecutionDataFromProcess(proc);
	if (!data) return;

	// Read whatever is currently available from stdout
	QString chunk = proc->readAllStandardOutput();

	// Emit the signal for partial output
	emit scriptOutput(data->executionId, chunk);
}
