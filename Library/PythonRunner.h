#pragma once

#include "global.h"
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QFuture>
#include <QPromise>
#include <QHash>
#include "PythonResult.h"

class LIBRARY_EXPORT PythonRunner : public QObject {
    Q_OBJECT
public:
    explicit PythonRunner(QObject* parent = nullptr);
    ~PythonRunner();


    QFuture<PythonResult> runScriptAsync(const QString& executionId, const QString& script, const QVariantList& arguments = {}, int timeout = -1);

    /**
     * @brief Cancels the execution of a script.
     * @param executionId The unique identifier of the script execution to cancel.
     * @return True if the execution was successfully canceled, false otherwise.
     */
    bool cancel(const QString& executionId);

signals:
    void scriptFinished(const QString& executionId, const PythonResult& result);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessErrorOccurred(QProcess::ProcessError error);
    void onTimeout();

private:
    QString pythonHome;
    QString pythonExecutablePath;
    QString getPythonExecutablePath() const;
	QString getSitePackagesPath() const;
	QString getDefaultEnvPath() const;

    struct ExecutionData {
        QString executionId;

        QProcess* process;
        QTimer* timer;
        QPromise<PythonResult> promise;
        QElapsedTimer* elapsedTimer;
    };

    QHash<QString, ExecutionData*> executions;

    void setupProcess(const QString& executionId, const QString& script, const QVariantList& arguments, int timeout);
	void cleanUpExecutionData(const QString& executionId, ExecutionData* data);

};
