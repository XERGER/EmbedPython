#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QFuture>
#include <memory>
#include "PythonEnvironment.h"
#include "PythonResult.h"

/**
 * @brief The PythonRunner class manages the execution of Python scripts.
 *        Implementation details are hidden using the Pimpl idiom.
 */
class LIBRARY_EXPORT PythonRunner : public QObject {
    Q_OBJECT
public:
    explicit PythonRunner(std::shared_ptr<PythonEnvironment> pythonInstance, QObject* parent = nullptr);
    virtual ~PythonRunner();

    // Synchronous script execution
    PythonResult runScript(const QString& script, const QVariantList& arguments = {}, int timeout = 0);

    // Asynchronous script execution
    QFuture<PythonResult> runScriptAsync(const QString& script, const QVariantList& arguments = {}, int timeout = 0);

    // Cancel the running script
    void cancel();

    // Fluent API Methods
    PythonRunner& setScript(const QString& script);
    PythonRunner& setArguments(const QVariantList& arguments);
    PythonRunner& setTimeout(int timeout);
    PythonResult execute();

    // Operator Overloading for Fluent API
    PythonRunner& operator()(const QString& script);
    PythonRunner& operator()(const QString& script, const QVariantList& arguments);
    PythonRunner& operator()(const QString& script, const QVariantList& arguments, int timeout);


private:
    // Use Pimpl idiom to hide implementation details
    class Impl;
    std::unique_ptr<Impl> impl; // Pointer to the implementation
};
