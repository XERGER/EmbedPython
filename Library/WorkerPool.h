#ifndef WORKERPOOL_H
#define WORKERPOOL_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVariant>
#include <QFuture>
#include <QFutureInterface>
#include <QQueue>
#include <QMutex>
#include <QMap>


//the problem with worker pools are that they are limited would need to start always one in addition
//second problem is the handling with pip install packages! 
class WorkerPool : public QObject {
	Q_OBJECT

public:
	explicit WorkerPool(const QString& workerPath, QObject* parent = nullptr);

	QFuture<QJsonObject> executeScript(const QString& executionId, const QString& script, const QVariantList& arguments);

private:
	struct Task {
		QString executionId;
		QString script;
		QVariantList arguments;

		QFutureInterface<QJsonObject> futureInterface;
	};

	QString workerScriptPath;
	QString token; // Unique token
	QList<QProcess*> availableWorkers;
	QQueue<Task> taskQueue;
	QMap<QProcess*, Task> activeTasks;
	QMutex taskQueueMutex;
	QMutex workerMutex;

	void prespawnWorkers(int count);
	void spawnWorker();
	void assignWorkerToTask();
	void handleWorkerOutput(QProcess* worker);
	void handleWorkerExit(QProcess* worker);
};

#endif // WORKERPOOL_H
