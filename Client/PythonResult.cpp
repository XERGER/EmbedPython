
#include "PythonResult.h"

PythonResult::PythonResult()
	: success(false), output(), errorOutput(), errorCode(0),  executionTime(0)
{
}

PythonResult::PythonResult(QString const& executionId, bool success, const QString& output, const QString& errorOutput,  qint64 executionTime)
	: executionId(executionId), success(success), output(output), errorOutput(errorOutput), errorCode(0),  executionTime(executionTime)
{
}

void PythonResult::setErrorCode(int code) {
	errorCode = code;
}
QString PythonResult::getOutput() const
{
	return output;
}

QString PythonResult::getErrorOutput() const
{
	return errorOutput;
}

QString PythonResult::getExecutionId() const
{
	return executionId;
}

int PythonResult::getErrorCode() const
{
	return errorCode;
}

bool PythonResult::isSuccess() const
{
	return success;
}

qint64 PythonResult::getExecutionTime() const
{
	return executionTime;
}

void PythonResult::setExecutionTime(qint64 time)
{
	executionTime = time;
}

QJsonObject PythonResult::toJson() const
{
	QJsonObject json;
	json["success"] = success;
	json["output"] = output;
	json["errorOutput"] = errorOutput;
	json["errorCode"] = errorCode;
	json["executionTime"] = executionTime;
	json["executionId"] = executionId;

	return json;
}
