
#include "PythonResult.h"

PythonResult::PythonResult()
	: success(false), output(), errorOutput(), errorCode(0), returnValue(), executionTime(0)
{
}

PythonResult::PythonResult(bool success, const QString& output, const QString& errorOutput, const QJsonObject& returnValue, qint64 executionTime)
	: success(success), output(output), errorOutput(errorOutput), errorCode(0), returnValue(returnValue), executionTime(executionTime)
{
}

QJsonObject PythonResult::getReturnValue() const
{
	return returnValue;
}

void PythonResult::setReturnValue(const QJsonObject& value)
{
	returnValue = value;
}

QString PythonResult::getOutput() const
{
	return output;
}

QString PythonResult::getErrorOutput() const
{
	return errorOutput;
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
	json["returnValue"] = returnValue;
	return json;
}
