// server.cpp
#include "server.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QJsonParseError>
#include <QJsonArray>
#include <QFutureWatcher>
#include <QFuture>
#include <QProcess>

// Include Python integration headers
#include "Library/PythonEnvironment.h"
#include "Library/PythonRunner.h"
#include "encryption.h"

Server::Server(QObject* parent)
	: QObject(parent),
	localServer(new QLocalServer(this)),
	pythonEnv(std::make_shared<PythonEnvironment>()),
	pythonRunner(std::make_unique<PythonRunner>(pythonEnv, this))
{
	// Connect the newConnection signal to the onNewConnection slot
	connect(localServer, &QLocalServer::newConnection, this, &Server::onNewConnection);


	if (pythonEnv->verifyPythonExecutable() && !pythonEnv->lockPythonExecutable()) {
		qCritical() << "Failed to initialize Python Environment.";
	}

	// Initialize Python Environment
	if (!pythonEnv->init()) {
		qCritical() << "Failed to initialize Python Environment.";
	}

	
}

Server::~Server()
{
	qDebug() << "Server shutting down...";
	// Clean up resources
	foreach(QLocalSocket * client, clients) {
		client->disconnectFromServer();
		client->deleteLater();
	}
	localServer->close();
}

void Server::startServer() {
	// Kill other executables named "PythonEngine"
#ifdef Q_OS_WIN
	QProcess::execute("taskkill /F /IM PythonEngine.exe /T");
#else
	QProcess::execute("pkill -f PythonEngine");
#endif

	// Generate a unique server name
	const auto serverName = Encryption::generateServerName();

	// Ensure no conflict by removing any existing server with the same name
	QLocalServer::removeServer(serverName);

	if (!localServer->listen(serverName)) {
		qCritical() << "Unable to start the server:" << localServer->errorString();
		return;
	}
	qDebug() << "Server started, listening on" << serverName;
}

void Server::onNewConnection()
{
	while (localServer->hasPendingConnections()) {
		const auto client = localServer->nextPendingConnection();

		connect(client, &QLocalSocket::readyRead, this, &Server::onReadyRead);
		connect(client, &QLocalSocket::disconnected, this, &Server::onDisconnected);

		clients.append(client);

		qDebug() << "New client connected.";
	}
}

void Server::onReadyRead()
{
	// Access sender() within the Server class, which inherits from QObject
	const auto client = qobject_cast<QLocalSocket*>(sender());

	if (!client)
		return;

	const auto data = client->readAll();
	processCommand(client, data);
}

QVariantList Server::deserializeVariantList(const QJsonArray& array) {
	QVariantList arguments;

	for (const QJsonValue& value : array)
		arguments.append(value.toVariant());
	
	return arguments;
}

QByteArray Server::encryptResponse(const QJsonObject& responseObj) {
 
	QJsonDocument responseDoc(responseObj);
	const auto plainData = responseDoc.toJson(QJsonDocument::Compact);

	QByteArray iv;
	const auto cipherText = Encryption::encryptData(plainData,  iv);
	if (cipherText.isEmpty()) {
		qWarning() << "Failed to encrypt response.";
		return {};
	}

	return iv + cipherText; // Prepend IV to the encrypted message
}

void Server::processCommand(QLocalSocket* client, const QByteArray& encryptedData) {
	const int ivSize = 16; // AES block size
	const auto iv = encryptedData.left(ivSize);
	const auto cipherText = encryptedData.mid(ivSize);

	const auto plainData = Encryption::decryptData(cipherText, iv);
	if (plainData.isEmpty()) {
		sendErrorResponse(client, "Decryption failed.");
		return;
	}

	QJsonParseError parseError;
	const auto doc = QJsonDocument::fromJson(plainData, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		sendErrorResponse(client, "Invalid JSON format.");
		return;
	}

	if (!doc.isObject()) {
		sendErrorResponse(client, "JSON data is not an object.");
		return;
	}

	const auto obj = doc.object();
	const auto command = obj["command"].toString();

	if (command == "execute") {
		handleExecuteCommand(client, obj);
	}
	else if (command == "installPackage") {
		handleInstallPackageCommand(client, obj);
	}
	else if (command == "installLocalPackage") {
		handleInstallLocalPackageCommand(client, obj);
	}
	else if (command == "installOpenAPIClient") {
		handleInstallOpenAPIClientCommand(client);
	}
	else if (command == "checkSyntax") {
		handleCheckSyntaxCommand(client, obj);
	}
	else {
		sendErrorResponse(client, "Unknown command.");
	}
}

void Server::handleCheckSyntaxCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto script = obj["script"].toString();
	const auto executionId = obj["executionId"].toString();

	if (script.isEmpty()) {
		sendErrorResponse(client, "Script is empty.", executionId);
		return;
	}

	PythonResult syntaxResult = pythonRunner->checkSyntax(script);
	QJsonObject responseObj;

	if (syntaxResult.isSuccess()) {
		responseObj["status"] = "success";
		responseObj["message"] = "Syntax is valid.";
	}
	else {
		responseObj["status"] = "error";
		responseObj["message"] = syntaxResult.getErrorOutput();
	}

	responseObj["executionId"] = executionId;
	sendResponse(client, responseObj);
}

void Server::handleExecuteCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto script = obj["script"].toString();
	const auto arguments = deserializeVariantList(obj["arguments"].toArray());
	const auto executionId = obj["executionId"].toString();
	const auto timeout = obj["timeout"].toInt(0);

	if (script.isEmpty()) {
		sendErrorResponse(client, "Script is empty.", executionId);
		return;
	}

	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}

	const auto future = pythonRunner->runScriptAsync(script, arguments, timeout);
	auto watcher = new QFutureWatcher<PythonResult>(this);
	connect(watcher, &QFutureWatcher<PythonResult>::finished, this,
		[this, watcher, client,  executionId, script, arguments]() {
			handleScriptExecutionResult(watcher, client, executionId, script, arguments); // reduce arguments, make the qjsonobject own struct so that i can just cmobine 3 to one
		});

	watcher->setFuture(future);
}

void Server::handleScriptExecutionResult(QFutureWatcher<PythonResult>* watcher, QLocalSocket* client, const QString& executionId, const QString& script,  const QVariantList& arguments) {
	const auto result = watcher->future().result();
	QJsonObject responseObj;

	if (!result.isSuccess()) {
		handleMissingModules(client, watcher, executionId, script, arguments, result);
		return;
	}

	responseObj["status"] = "success";
	responseObj["stdout"] = result.getOutput();
	responseObj["stderr"] = result.getErrorOutput();
	responseObj["executionTime"] = result.getExecutionTime();
	responseObj["result"] = result.getReturnValue();
	responseObj["executionId"] = executionId;

	sendResponse(client, responseObj);
	watcher->deleteLater();
}

void Server::handleMissingModules(QLocalSocket* client, QFutureWatcher<PythonResult>* watcher, const QString& executionId, const QString& script, const QVariantList& arguments, const PythonResult& result) {
	QStringList missingModules;
	QRegularExpression moduleRegex(R"((?<=No module named ')[^']+)");
	auto matches = moduleRegex.globalMatch(result.getErrorOutput());

	while (matches.hasNext()) {
		missingModules.append(matches.next().captured(0));
	}

	// Map of incorrect imports to correct imports
	QMap<QString, QString> importFixes = {
		   {"bs4", "beautifulsoup4"},
		   {"PIL", "Pillow"},
		   {"yaml", "PyYAML"},
		   {"cv2", "opencv-python"},
		   {"dateutil", "python-dateutil"},
		   {"dotenv", "python-dotenv"},
		   {"websocket", "websocket-client"},
		   {"sklearn", "scikit-learn"},
		   {"Crypto", "pycryptodome"},
		   {"setuptools_scm", "setuptools-scm"},
		   {"gi", "PyGObject"},
		   {"psycopg2", "psycopg2-binary"},
		   {"mpl_toolkits", "matplotlib"},
		   {"flask_sqlalchemy", "Flask-SQLAlchemy"},
		   {"jinja2", "Jinja2"},
		   {"werkzeug", "Werkzeug"},
		   {"twisted", "Twisted"},
		   {"kivy", "Kivy"},
		   {"scrapy", "Scrapy"}
	};

	for (const auto& module : missingModules) {
		const auto correctedModule = importFixes.value(module, module); // Default to the module itself if not in map

		if (!pythonEnv->installPackage(correctedModule)) {
			sendErrorResponse(client, QString("Failed to install module: %1").arg(correctedModule), executionId);
			watcher->deleteLater();
			return;
		}
	}

	retryScriptExecution(client, watcher, executionId, script, arguments);
}


void Server::retryScriptExecution(QLocalSocket* client, QFutureWatcher<PythonResult>* watcher, const QString& executionId, const QString& script, const QVariantList& arguments) {
	const auto future = pythonRunner->runScriptAsync(script, arguments);
	auto retryWatcher = new QFutureWatcher<PythonResult>(this);

	connect(retryWatcher, &QFutureWatcher<PythonResult>::finished, this,
		[this, retryWatcher, client, executionId]() {
			const auto retryResult = retryWatcher->future().result();
			QJsonObject responseObj;

			responseObj["status"] = retryResult.isSuccess() ? "success" : "error";
			responseObj["stdout"] = retryResult.getOutput();
			responseObj["stderr"] = retryResult.getErrorOutput();
			responseObj["executionTime"] = retryResult.getExecutionTime();
			responseObj["result"] = retryResult.getReturnValue();
			responseObj["executionId"] = executionId;

			sendResponse(client, responseObj); // make the client an own class so that i can write client->senResponse(responseObj) #TODO
			retryWatcher->deleteLater();
		});

	retryWatcher->setFuture(future);
	watcher->deleteLater();
}

void Server::handleInstallPackageCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto package = obj["package"].toString();
	QJsonObject responseObj;

	if (package.isEmpty()) {
		sendErrorResponse(client, "Package name is empty.");
		return;
	}

	const auto success = pythonEnv->installPackage(package);
	responseObj["status"] = success ? "success" : "error";
	responseObj["stdout"] = success ? "Package installed successfully." : "Failed to install package.";
	sendResponse(client, responseObj);
}

void Server::handleInstallLocalPackageCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto packagePath = obj["packagePath"].toString();
	QJsonObject responseObj;

	if (packagePath.isEmpty()) {
		sendErrorResponse(client, "Package path is empty.");
		return;
	}

	const auto success = pythonEnv->installLocalPackage(packagePath);
	responseObj["status"] = success ? "success" : "error";
	responseObj["stdout"] = success ? "Package installed successfully." : "Failed to install package.";
	responseObj["executionId"] = packagePath;

	sendResponse(client, responseObj);
}

void Server::handleInstallOpenAPIClientCommand(QLocalSocket* client) {
	const auto success = pythonEnv->installOpenAPIClient();
	QJsonObject responseObj;

	responseObj["status"] = success ? "success" : "error";
	responseObj["stdout"] = success ? "OpenAPI client installed successfully." : "Failed to install OpenAPI client.";
	sendResponse(client, responseObj);
}

void Server::sendErrorResponse(QLocalSocket* client, const QString& message, const QString& executionId ) {
	QJsonObject responseObj;
	responseObj["status"] = "error";
	responseObj["stdout"] = message;
	if (!executionId.isEmpty()) {
		responseObj["executionId"] = executionId;
	}

	sendResponse(client, responseObj);
}

void Server::sendResponse(QLocalSocket* client, const QJsonObject& responseObj) {
	const auto responseEncrypted = encryptResponse(responseObj);
	client->write(responseEncrypted);
}


void Server::onDisconnected()
{
	// Access sender() within the Server class, which inherits from QObject
	const auto client = qobject_cast<QLocalSocket*>(sender());
	if (!client)
		return;

	clients.removeAll(client);
	client->deleteLater();

	qDebug() << "Client disconnected.";
}
