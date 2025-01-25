// PythonSyntaxCheck.h
#pragma once

#include "global.h"
#include <QObject>
#include <QFuture>
#include "PythonResult.h"
#include "PythonRunner.h"

class LIBRARY_EXPORT PythonSyntaxCheck : public QObject {
    Q_OBJECT
public:
    explicit PythonSyntaxCheck(QObject* parent = nullptr);
    ~PythonSyntaxCheck();

    /**
     * @brief Initiates an asynchronous syntax check for a Python script provided as a string.
     * @param executionId A unique identifier for the execution instance.
     * @param script The Python script content to be checked.
     * @param timeout Optional timeout in milliseconds. Use -1 for no timeout.
     * @return A QFuture representing the asynchronous syntax check result.
     */
    QFuture<PythonResult> checkSyntaxAsync(const QString& executionId, const QString& script, int timeout = -1);

    /**
     * @brief Initiates an asynchronous syntax check for a Python script provided via a file path.
     * @param executionId A unique identifier for the execution instance.
     * @param filePath The path to the Python script file to be checked.
     * @param timeout Optional timeout in milliseconds. Use -1 for no timeout.
     * @return A QFuture representing the asynchronous syntax check result.
     */
    QFuture<PythonResult> checkFileSyntaxAsync(const QString& executionId, const QString& filePath, int timeout = -1);

    /**
     * @brief Cancels the syntax check execution associated with the given executionId.
     * @param executionId The unique identifier of the syntax check execution to cancel.
     * @return True if the execution was successfully canceled, false otherwise.
     */
    bool cancel(const QString& executionId);

signals:
    /**
     * @brief Emitted when a syntax check operation finishes.
     * @param executionId The unique identifier for the execution.
     * @param result The result of the syntax check operation.
     */
    void syntaxCheckFinished(const QString& executionId, const PythonResult& result);

    /**
     * @brief Emitted to indicate progress during the syntax check operation.
     * @param executionId The unique identifier for the execution.
     * @param progressMessage A message describing the current progress.
     */
    void syntaxCheckProgress(const QString& executionId, const QString& progressMessage);

private:
    PythonRunner* runner; // Instance of PythonRunner to execute commands

    /**
     * @brief Constructs a Python command to check syntax of a script string.
     * @param script The Python script content to be checked.
     * @return A Python command string that attempts to compile the script.
     */
    QString constructSyntaxCheckCommand(const QString& script) const;
};
