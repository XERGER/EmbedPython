// PythonSyntaxCheck.cpp
#include "PythonSyntaxCheck.h"
#include <QFile>
#include <QDebug>

// Constructor
PythonSyntaxCheck::PythonSyntaxCheck(QObject* parent)
	: QObject(parent),
	runner(new PythonRunner(this))
{
	// Connect the runner's finished signal to this class's finished signal
	connect(runner, &PythonRunner::scriptFinished, this, &PythonSyntaxCheck::syntaxCheckFinished);

}

// Destructor
PythonSyntaxCheck::~PythonSyntaxCheck() {
	// PythonRunner will be deleted automatically as it's a child
}

// Public Methods

QFuture<PythonResult> PythonSyntaxCheck::checkSyntaxAsync(const QString& executionId, const QString& script, int timeout) {
	// Construct the Python command to check syntax
	QString command = constructSyntaxCheckCommand(script);

	// Connect to progress signals (if any)
	// Currently, PythonRunner emits executionFinished which is connected above

	// Execute the command using PythonRunner
	QFuture<PythonResult> future = runner->runScriptAsync(executionId, command, {}, timeout);

	// Emit progress if necessary
	emit syntaxCheckProgress(executionId, "Syntax check started.");

	return future;
}

QFuture<PythonResult> PythonSyntaxCheck::checkFileSyntaxAsync(const QString& executionId, const QString& filePath, int timeout) {
	// Read the file content
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QString error = QString("Failed to open file '%1' for reading.").arg(filePath);
		qCritical() << error;
		PythonResult result(executionId, false, QString(), error, 0);
		QPromise<PythonResult> promise;
		promise.addResult(result);
		promise.finish();
		return promise.future();
	}

	QString script = QString::fromUtf8(file.readAll());
	file.close();

	// Reuse the script-based syntax check
	return checkSyntaxAsync(executionId, script, timeout);
}

bool PythonSyntaxCheck::cancel(const QString& executionId) {
	return runner->cancel(executionId);
}

// Private Methods

QString PythonSyntaxCheck::constructSyntaxCheckCommand(const QString& script) const {
	// Escape backslashes and double quotes in the script
	QString escapedScript = script;
	escapedScript.replace("\\", "\\\\");
	escapedScript.replace("\"", "\\\"");

	// Construct the command to compile the script
	// This command will attempt to compile the script and raise an error if there's a syntax issue
	QString command = QString(
		"import sys; "
		"script = \"\"\"%1\"\"\"; "
		"try: compile(script, '<string>', 'exec'); "
		"except SyntaxError as e: "
		"    print(f'SyntaxError: {e.msg} at line {e.lineno}'); "
		"    sys.exit(1)"
	).arg(escapedScript);

	return command;
}
