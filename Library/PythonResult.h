#pragma once
#include <QVariant>
#include <QString>
#include <QMetaType>
#include <QJsonObject>
#include "global.h"

/**
 * @brief Encapsulates the result of Python script execution.
 */
class LIBRARY_EXPORT PythonResult
{
public:
    PythonResult();
    PythonResult(bool success, const QString& output, const QString& errorOutput, const QJsonObject& returnValue = QJsonObject(), qint64 executionTime = 0);

    QJsonObject getReturnValue() const;
    void setReturnValue(const QJsonObject& value);

    QString getOutput() const;
    QString getErrorOutput() const;
    int getErrorCode() const;
    bool isSuccess() const;

    qint64 getExecutionTime() const;
    void setExecutionTime(qint64 time);

    /**
     * @brief Converts the PythonResult into a QJsonObject for easy JSON manipulation.
     * @return A QJsonObject representing the result.
     */
    QJsonObject toJson() const;

private:
    bool success;
    QString output;
    QString errorOutput;
    int errorCode;
    QJsonObject returnValue;
    qint64 executionTime; // Execution time in milliseconds
};

// Enable PythonResult to be used in Qt's signal-slot mechanism
Q_DECLARE_METATYPE(PythonResult)
