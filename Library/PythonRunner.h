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

	void scriptOutput(const QString& executionId, const QString& message);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessErrorOccurred(QProcess::ProcessError error);
    void onTimeout();
	void onProcessReadyReadStandardOutput();

private:

    QString pythonHome;
    QString pythonExecutablePath;
    QProcessEnvironment environment;
    QString getPythonExecutablePath() const;
	QString getSitePackagesPath() const;
	QString getDefaultEnvPath() const;

    QProcessEnvironment createEnviornment() const;
    struct ExecutionData {
        QString executionId;

        QProcess* process;
        QTimer* timer;
        QPromise<PythonResult> promise;
        QElapsedTimer* elapsedTimer;
    };

    QHash<QString, ExecutionData*> executions;


	void cleanUpExecutionData(const QString& executionId, ExecutionData* data);

    ExecutionData* getExecutionDataFromProcess(QProcess* process) const;
};
