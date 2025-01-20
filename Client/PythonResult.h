#pragma once
#include "global.h"
#include <QVariant>
#include <QMetaType>
#include <QJsonObject>
enum class OperationType {
	Install,
	Reinstall,
	Update,
	InstallLocal,
	UpdateLocal,
	Uninstall,
	UpgradeAll,
	Search
};
/**
 * @brief Encapsulates the result of Python script execution.
 */
class PYTHHONCLIENT_EXPORT PythonResult
{
public:
    PythonResult();
    PythonResult(QString const& executionId, bool success, const QString& output, const QString& errorOutput, qint64 executionTime = 0);


    QString getOutput() const;
    QString getExecutionId()const;
    QString getErrorOutput() const;
    int getErrorCode() const;
    bool isSuccess() const;

    qint64 getExecutionTime() const;
    void setExecutionTime(qint64 time);
    void setErrorCode(int code);
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
    qint64 executionTime; // Execution time in milliseconds
    QString executionId;
};

// Enable PythonResult to be used in Qt's signal-slot mechanism
Q_DECLARE_METATYPE(PythonResult)
