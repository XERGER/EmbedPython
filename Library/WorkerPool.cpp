#include "WorkerPool.h"
#include <QJsonArray>
#include <QDebug>
#include <QCryptographicHash>

QString generateHash() {
	// Retrieve system-specific identifiers
	QString machineHostName = QSysInfo::machineHostName();
	QString bootUniqueId = QSysInfo::bootUniqueId(); // Requires Qt 6
	QString kernelType = QSysInfo::kernelType();
	QString kernelVersion = QSysInfo::kernelVersion();

	// Combine identifiers into a unique string
	QString uniqueString = machineHostName + bootUniqueId + kernelType + kernelVersion;

	// Hash the unique string using SHA-256
	QByteArray hash = QCryptographicHash::hash(uniqueString.toUtf8(), QCryptographicHash::Sha256);

	// Convert to a hexadecimal string
	return hash.toHex();
}

WorkerPool::WorkerPool(const QString& workerPath, QObject* parent)
	: QObject(parent), workerScriptPath(workerPath), token(generateHash()) {
	prespawnWorkers(2); // Pre-spawn 2 workers
}

void WorkerPool::prespawnWorkers(int count) {
	for (int i = 0; i < count; ++i) {
		spawnWorker();
	}
}

void WorkerPool::spawnWorker() {
	QProcess* worker = new QProcess(this);
	worker->setProgram(workerScriptPath);

	connect(worker, &QProcess::readyReadStandardOutput, this, [this, worker]() {
		handleWorkerOutput(worker);
		});

	connect(worker, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [this, worker](int, QProcess::ExitStatus) {
			handleWorkerExit(worker);
		});

	worker->start();
	if (worker->waitForStarted()) {
		QMutexLocker locker(&workerMutex);
		availableWorkers.append(worker);
	}
	else {
		qWarning() << "Failed to start worker process.";
		worker->deleteLater();
	}
}

void WorkerPool::assignWorkerToTask() {
	QMutexLocker taskLocker(&taskQueueMutex);
	QMutexLocker workerLocker(&workerMutex);

	if (taskQueue.isEmpty() || availableWorkers.isEmpty()) {
		return;
	}

	QProcess* worker = availableWorkers.takeFirst();
	Task task = taskQueue.dequeue();
	activeTasks[worker] = task;

	QJsonObject inputObj;
	inputObj["script"] = task.script;

	QJsonArray argsArray;
	for (const auto& arg : task.arguments) {
		argsArray.append(QJsonValue::fromVariant(arg));
	}
	inputObj["arguments"] = argsArray;
	inputObj["token"] = token; // Add the token
	inputObj["command"] = "execute";

	QByteArray inputData = QJsonDocument(inputObj).toJson(QJsonDocument::Compact) + "\n";
	worker->write(inputData);
	worker->closeWriteChannel();
}

QFuture<QJsonObject> WorkerPool::executeScript(const QString& executionId, const QString& script, const QVariantList& arguments) {
	QFutureInterface<QJsonObject> futureInterface;
	QFuture<QJsonObject> future = futureInterface.future();

	{
		QMutexLocker locker(&taskQueueMutex);
		taskQueue.enqueue({ executionId, script, arguments, futureInterface });
	}

	assignWorkerToTask();
	return future;
}

void WorkerPool::handleWorkerOutput(QProcess* worker) {
	QByteArray outputData = worker->readAllStandardOutput();
	QJsonDocument outputDoc = QJsonDocument::fromJson(outputData);

	if (!outputDoc.isObject()) {
		qWarning() << "Invalid output from worker:" << outputData;
		return;
	}

	QJsonObject result = outputDoc.object();

	if (activeTasks.contains(worker)) {
		Task task = activeTasks.take(worker);
		task.futureInterface.reportFinished(&result);
	}

	{
		QMutexLocker locker(&workerMutex);
		availableWorkers.append(worker);
	}

	assignWorkerToTask();
}

void WorkerPool::handleWorkerExit(QProcess* worker) {
	{
		QMutexLocker locker(&workerMutex);
		availableWorkers.removeOne(worker);
	}

	if (activeTasks.contains(worker)) {
		Task task = activeTasks.take(worker);
		QJsonObject errorResult;
		errorResult["success"] = false;
		errorResult["error"] = "Worker process terminated unexpectedly.";
		task.futureInterface.reportFinished(&errorResult);
	}

	worker->deleteLater();
}
