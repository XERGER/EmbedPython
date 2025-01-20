// server.cpp

#include "server.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QJsonParseError>
#include <QJsonArray>
#include <QFutureWatcher>
#include <QFuture>
#include <QFileInfo>
#include <QProcess>
// Include Python integration headers
#include "Library/PythonEnvironment.h"
#include "Library/PythonRunner.h"
#include "encryption.h"


// Constructor
Server::Server(QObject* parent)
	: QObject(parent),
	localServer(new QLocalServer(this)),
	pythonEnv(std::make_shared<PythonEnvironment>()),
	pythonRunner(std::make_unique<PythonRunner>( this)) {

	connect(localServer, &QLocalServer::newConnection, this, &Server::onNewConnection);
	connect(pythonEnv.get(), &PythonEnvironment::packageOperationFinished, this, &Server::onPackageOperationFinished);
	connect(pythonEnv.get(), &PythonEnvironment::packageOperationProgress, this, &Server::onPackageOperationProgress);
}

// Destructor
Server::~Server() {
	qDebug() << "Server shutting down...";
	// Clean up resources
	foreach(QLocalSocket * client, clients) {
		client->disconnectFromServer();
		client->deleteLater();
	}
	localServer->close();
}

// Start Server
void Server::startServer() {

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

// Handle New Connections
void Server::onNewConnection() {
	while (localServer->hasPendingConnections()) {
		const auto client = localServer->nextPendingConnection();
		connect(client, &QLocalSocket::readyRead, this, &Server::onReadyRead);
		connect(client, &QLocalSocket::disconnected, this, &Server::onDisconnected);
		clients.append(client);
		qDebug() << "New client connected.";
	}
}

// Handle Incoming Data
void Server::onReadyRead() {
	// Access sender() within the Server class, which inherits from QObject
	const auto client = qobject_cast<QLocalSocket*>(sender());
	if (!client)
		return;

	bufferMap[client].append(client->readAll());

	while (true) {
		QByteArray& buffer = bufferMap[client];

		// Check if we have enough data to read the length prefix
		if (buffer.size() < 4) {
			// Not enough data to determine message length
			break;
		}

		// Read the length prefix (first 4 bytes)
		QDataStream stream(buffer.left(4));
		stream.setByteOrder(QDataStream::BigEndian);
		quint32 messageLength;
		stream >> messageLength;

		// Check for invalid message length
		if (messageLength == 0 || messageLength > (100 * 1024 * 1024)) { // 100 MB limit
			qWarning() << "Invalid message length:" << messageLength;
			// Optionally, disconnect the client
			client->disconnectFromServer();
			bufferMap.remove(client);
			break;
		}

		// Check if the entire message has been received
		if (buffer.size() < 4 + messageLength) {
			// Wait for more data
			break;
		}

		// Extract the complete encrypted message
		QByteArray encryptedData = buffer.mid(4, messageLength);
		buffer.remove(0, 4 + messageLength); // Remove processed data from buffer

		// Decrypt the data
		const int ivSize = 16; // AES block size
		if (encryptedData.size() < ivSize) {
			sendErrorResponse(client, "Encrypted data is too short.", "");
			continue;
		}

		QByteArray iv = encryptedData.left(ivSize); // Extract the IV
		QByteArray cipherText = encryptedData.mid(ivSize);
		QByteArray plainData = Encryption::decryptData(cipherText, iv);

		if (plainData.isEmpty()) {
			sendErrorResponse(client, "Decryption failed.", "");
			continue;
		}

		// Parse the decrypted data as JSON
		QJsonParseError parseError;
		QJsonDocument doc = QJsonDocument::fromJson(plainData, &parseError);

		if (doc.isNull()) {
			sendErrorResponse(client, "JSON parse error: " + parseError.errorString(), "");
			qDebug() << "Received plain data:" << plainData;
			continue;
		}

		if (!doc.isObject()) {
			sendErrorResponse(client, "Received JSON is not an object.", "");
			qDebug() << "Received JSON:" << doc;
			continue;
		}

		QJsonObject obj = doc.object();
		processCommand(client, obj);
	}
}


// Deserialize QVariantList
QVariantList Server::deserializeVariantList(const QJsonArray& array) {
	QVariantList arguments;
	for (const QJsonValue& value : array)
		arguments.append(value.toVariant());
	return arguments;
}

// Encrypt Response
QByteArray Server::encryptResponse(const QJsonObject& responseObj) {
	QJsonDocument responseDoc(responseObj);
	const auto plainData = responseDoc.toJson(QJsonDocument::Compact);
	QByteArray iv;
	const auto cipherText = Encryption::encryptData(plainData, iv);
	if (cipherText.isEmpty()) {
		qWarning() << "Failed to encrypt response.";
		return {};
	}
	return iv + cipherText; // Prepend IV to the encrypted message
}

// Process Incoming Command
void Server::processCommand(QLocalSocket* client, const QJsonObject& obj) {
	

	const auto command = obj["command"].toString();

	if (command.isEmpty()) {
		sendErrorResponse(client, "Command is missing.");
		return;
	}

	static const QMap<QString, std::function<void(QLocalSocket*, const QJsonObject&)>> commandHandlers = {
		{"execute", [&](QLocalSocket* c, const QJsonObject& o) { handleExecuteCommand(c, o); }},
		{"installPackage", [&](QLocalSocket* c, const QJsonObject& o) { handleInstallPackageCommand(c, o); }},
		{"uninstallPackage", [&](QLocalSocket* c, const QJsonObject& o) { handleUninstallPackageCommand(c, o); }},
		{"reinstallPackage", [&](QLocalSocket* c, const QJsonObject& o) { handleReinstallPackageCommand(c, o); }},
		{"updatePackage", [&](QLocalSocket* c, const QJsonObject& o) { handleUpdatePackageCommand(c, o); }},
		{"installLocalPackage", [&](QLocalSocket* c, const QJsonObject& o) { handleInstallLocalPackageCommand(c, o); }},
		{"updateLocalPackage", [&](QLocalSocket* c, const QJsonObject& o) { handleUpdateLocalPackageCommand(c, o); }},
		{"checkSyntax", [&](QLocalSocket* c, const QJsonObject& o) { handleCheckSyntaxCommand(c, o); }},
		{"upgradeAllPackages", [&](QLocalSocket* c, const QJsonObject& o) { handleUpgradeAllPackagesCommand(c, o); }},
		{"searchPackage", [&](QLocalSocket* c, const QJsonObject& o) { handleSearchPackageCommand(c, o); }},
		{"getPackageInfo", [&](QLocalSocket* c, const QJsonObject& o) { handleGetPackageInfoCommand(c, o); }},
		{"listInstalledPackages", [&](QLocalSocket* c, const QJsonObject& o) { handleListInstalledPackagesCommand(c, o); }},
		{"cancel", [&](QLocalSocket* c, const QJsonObject& o) { handleCancelCommand(c, o); }}
	};

	auto handler = commandHandlers.value(command, nullptr);
	if (handler) {
		handler(client, obj);
	}
	else {
		sendErrorResponse(client, "Unknown command.");
	}
}

void Server::handleListInstalledPackagesCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto executionId = obj["executionId"].toString();

	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}

	QStringList installedPackages = pythonEnv->listInstalledPackages();
	QJsonArray packagesArray;
	for (const QString& pkg : installedPackages) {
		packagesArray.append(pkg);
	}

	QJsonObject responseObj;
	responseObj["status"] = "success";
	responseObj["installedPackages"] = packagesArray;
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = false;
	sendResponse(client, responseObj);
}
void Server::handleGetPackageInfoCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto package = obj["package"].toString();
	const auto executionId = obj["executionId"].toString();

	if (package.isEmpty()) {
		sendErrorResponse(client, "Package name is empty.", executionId);
		return;
	}

	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}

	QJsonObject packageInfo = pythonEnv->getPackageInfo(package);
	if (packageInfo.isEmpty()) {
		sendErrorResponse(client, QString("Failed to retrieve information for package '%1'.").arg(package), executionId);
		return;
	}

	QJsonObject responseObj;
	responseObj["status"] = "success";
	responseObj["packageInfo"] = packageInfo;
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = false;
	sendResponse(client, responseObj);
}


void Server::handleUpgradeAllPackagesCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto executionId = obj["executionId"].toString();
	QJsonObject responseObj;

	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}

	// Initiate asynchronous upgrade of all packages
	pythonEnv->upgradeAllPackages();

	// Send initial response indicating upgrade has started
	responseObj["status"] = "started";
	responseObj["message"] = "Upgrade of all packages started.";
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = false;
	sendResponse(client, responseObj);
}

void Server::handleSearchPackageCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto query = obj["query"].toString();
	const auto executionId = obj["executionId"].toString();
	QJsonObject responseObj;

	if (query.isEmpty()) {
		sendErrorResponse(client, "Search query is empty.", executionId);
		return;
	}

	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}

	// Perform the search
	QStringList searchResults = pythonEnv->searchPackage(query);

	// Prepare the response
	responseObj["status"] = "success";
	QJsonArray resultsArray;
	for (const QString& packageName : searchResults) {
		resultsArray.append(packageName);
	}
	responseObj["results"] = resultsArray;
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = false;
	sendResponse(client, responseObj);
}

void Server::handleUninstallPackageCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto package = obj["package"].toString();
	const auto executionId = obj["executionId"].toString();
	QJsonObject responseObj;

	if (package.isEmpty()) {
		sendErrorResponse(client, "Package name is empty.", executionId);
		return;
	}

	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}

	// Check if the package is installed
	if (!pythonEnv->isPackageInstalled(package)) {
		sendErrorResponse(client, QString("Package '%1' is not installed.").arg(package), executionId);
		return;
	}

	// Initiate asynchronous uninstallation
	pythonEnv->uninstallPackage(executionId, package);

	// Send initial response indicating uninstallation has started
	responseObj["status"] = "started";
	responseObj["message"] = QString("Uninstallation of package '%1' started.").arg(package);
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = false;
	sendResponse(client, responseObj);
}
// Handle Check Syntax Command
void Server::handleCheckSyntaxCommand(QLocalSocket* client, const QJsonObject& obj) {
// 	const auto script = obj["script"].toString();
// 	const auto executionId = obj["executionId"].toString();
// 	if (script.isEmpty()) {
// 		sendErrorResponse(client, "Script is empty.", executionId);
// 		return;
// 	}
// 	PythonResult syntaxResult = pythonRunner->checkSyntax(script);
// 	QJsonObject responseObj;
// 	if (syntaxResult.isSuccess()) {
// 		responseObj["status"] = "success";
// 		responseObj["message"] = "Syntax is valid.";
// 	}
// 	else {
// 		responseObj["status"] = "error";
// 		responseObj["message"] = syntaxResult.getErrorOutput();
// 	}
// 	responseObj["executionId"] = executionId;
// 	sendResponse(client, responseObj);
}

// Handle Cancel Command
void Server::handleCancelCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto executionId = obj["executionId"].toString();

	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}

	// Cancel the specific script execution
	pythonRunner->cancel(executionId);

	// Respond to the client
	QJsonObject responseObj;
	responseObj["status"] = "cancelled";
	responseObj["message"] = QString("Execution with ID '%1' has been cancelled.").arg(executionId);
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = false;
	sendResponse(client, responseObj);
}

// Handle Execute Command
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
	const auto future = pythonRunner->runScriptAsync(executionId, script, arguments, timeout);
	auto watcher = new QFutureWatcher<PythonResult>(this);
	connect(watcher, &QFutureWatcher<PythonResult>::finished, this,
		[this, watcher, client, executionId, script, arguments]() {
			handleScriptExecutionResult(watcher, client, executionId, script, arguments);
		});
	watcher->setFuture(future);
}

// Handle Script Execution Result
void Server::handleScriptExecutionResult(QFutureWatcher<PythonResult>* watcher, QLocalSocket* client, const QString& executionId, const QString& script, const QVariantList& arguments) {
	const auto result = watcher->future().result();
	QJsonObject responseObj;
// 	if (!result.isSuccess()) {
// 		handleMissingModules(client, watcher, executionId, script, arguments, result);
// 		return;
// 	}


	responseObj["status"] = result.isSuccess() ? "success" : "error";
	responseObj["stdout"] = result.getOutput();
	responseObj["stderr"] = result.getErrorOutput();
	responseObj["executionTime"] = result.getExecutionTime();
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = true;

	sendResponse(client, responseObj);
	watcher->deleteLater();
}

// Handle Missing Modules
// void Server::handleMissingModules(QLocalSocket* client, QFutureWatcher<PythonResult>* watcher, const QString& executionId, const QString& script, const QVariantList& arguments, const PythonResult& result) {
// 	QStringList missingModules;
// 	QRegularExpression moduleRegex(R"((?<=No module named ')[^']+)");
// 
// 
// 	auto matches = moduleRegex.globalMatch(result.getErrorOutput());
// 	while (matches.hasNext()) {
// 		missingModules.append(matches.next().captured(0));
// 	}
// 
// 	// Map of incorrect imports to correct package names
// 	QMap<QString, QString> importFixes = {
// 		{"bs4", "beautifulsoup4"},
// 		{"PIL", "Pillow"},
// 		{"yaml", "PyYAML"},
// 		{"cv2", "opencv-python"},
// 		{"dateutil", "python-dateutil"},
// 		{"dotenv", "python-dotenv"},
// 		{"websocket", "websocket-client"},
// 		{"sklearn", "scikit-learn"},
// 		{"Crypto", "pycryptodome"},
// 		{"setuptools_scm", "setuptools-scm"},
// 		{"gi", "PyGObject"},
// 		{"psycopg2", "psycopg2-binary"},
// 		{"mpl_toolkits", "matplotlib"},
// 		{"flask_sqlalchemy", "Flask-SQLAlchemy"},
// 		{"jinja2", "Jinja2"},
// 		{"werkzeug", "Werkzeug"},
// 		{"twisted", "Twisted"},
// 		{"kivy", "Kivy"},
// 		{"scrapy", "Scrapy"}
// 	};
// 
// 	QStringList packagesToInstall;
// 	for (const QString& module : missingModules) {
// 		const QString correctedPackage = importFixes.value(module, module); // Default to the module itself if not in map
// 		packagesToInstall.append(correctedPackage);
// 	}
// 
// 	if (packagesToInstall.isEmpty()) {
// 		sendErrorResponse(client, "No valid packages found to install." + result.getErrorOutput()+ " | " + missingModules.join(";"), executionId);
// 		watcher->deleteLater();
// 		return;
// 	}
// 
// 	// Store execution data in the map
// 	executionMap[executionId] = ExecutionData(client, script, arguments, packagesToInstall);
// 
// 	// Initiate installation of each package
// 	for (const QString& package : packagesToInstall) {
// 		pythonEnv->installPackage(package);
// 	}
// 
// 	// Optionally, send a response indicating that installation has started
// 	QJsonObject responseObj;
// 	responseObj["status"] = "installing";
// 	responseObj["message"] = QString("Installation of packages '%1' started.").arg(packagesToInstall.join(", "));
// 	responseObj["executionId"] = executionId;
// 	responseObj["isScript"] = false;
// 	sendResponse(client, responseObj);
// }

// Handle Install Package Command
void Server::handleInstallPackageCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto package = obj["package"].toString();
	const auto executionId = obj["executionId"].toString();
	QJsonObject responseObj;
	if (package.isEmpty()) {
		sendErrorResponse(client, "Package name is empty.");
		return;
	}
	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}
	// Initiate asynchronous installation
	pythonEnv->installPackage(executionId, package);
	// Send initial response indicating installation has started
	responseObj["status"] = "started";
	responseObj["message"] = QString("Installation of package '%1' started.").arg(package);
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = false;
	sendResponse(client, responseObj);
}


// Handle Reinstall Package Command
void Server::handleReinstallPackageCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto package = obj["package"].toString();
	const auto executionId = obj["executionId"].toString();
	QJsonObject responseObj;
	if (package.isEmpty()) {
		sendErrorResponse(client, "Package name is empty.");
		return;
	}
	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}
	// Initiate asynchronous reinstallation
	pythonEnv->reinstallPackage(executionId, package);
	// Send initial response indicating reinstallation has started
	responseObj["status"] = "started";
	responseObj["message"] = QString("Reinstallation of package '%1' started.").arg(package);
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = false;
	sendResponse(client, responseObj);
}

// Handle Update Package Command
void Server::handleUpdatePackageCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto package = obj["package"].toString();
	const auto executionId = obj["executionId"].toString();
	QJsonObject responseObj;
	if (package.isEmpty()) {
		sendErrorResponse(client, "Package name is empty.");
		return;
	}
	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}
	// Initiate asynchronous update
	pythonEnv->updatePackage(executionId, package);
	// Send initial response indicating update has started
	responseObj["status"] = "started";
	responseObj["message"] = QString("Update of package '%1' started.").arg(package);
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = false;
	sendResponse(client, responseObj);
}

// Handle Update Local Package Command
void Server::handleUpdateLocalPackageCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto packagePath = obj["packagePath"].toString();
	const auto executionId = obj["executionId"].toString();
	QJsonObject responseObj;
	if (packagePath.isEmpty()) {
		sendErrorResponse(client, "Package path is empty.");
		return;
	}
	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}
	// Initiate asynchronous update of local package
	pythonEnv->updateLocalPackage(executionId, packagePath);
	// Send initial response indicating update has started
	responseObj["status"] = "started";
	responseObj["message"] = QString("Update of local package '%1' started.").arg(packagePath);
	responseObj["executionId"] = executionId;
	responseObj["isScript"] = false;
	sendResponse(client, responseObj);
}

// Send Error Response
void Server::sendErrorResponse(QLocalSocket* client, const QString& message, const QString& executionId) {
	QJsonObject responseObj;
	responseObj["status"] = "error";
	responseObj["stdout"] = message;
	if (!executionId.isEmpty()) {
		responseObj["executionId"] = executionId;
	}
	responseObj["isScript"] = false;


	sendResponse(client, responseObj);
}

// Send Response
void Server::sendResponse(QLocalSocket* client, const QJsonObject& responseObj) {
	// 1. Serialize JSON to QByteArray
	QByteArray plainData = QJsonDocument(responseObj).toJson(QJsonDocument::Compact);

	// 2. Encrypt the data
	QByteArray iv;
	QByteArray cipherText = Encryption::encryptData(plainData, iv);
	if (cipherText.isEmpty()) {
		qWarning() << "Failed to encrypt response.";
		return;
	}

	// 3. Prepend IV to ciphertext
	QByteArray encryptedData = iv + cipherText;

	// 4. Calculate length of encrypted data
	quint32 dataLength = encryptedData.size();

	// 5. Convert length to 4-byte big-endian
	QByteArray lengthPrefix;
	QDataStream stream(&lengthPrefix, QIODevice::WriteOnly);
	stream.setByteOrder(QDataStream::BigEndian);
	stream << dataLength;

	// 6. Send length prefix followed by encrypted data
	client->write(lengthPrefix + encryptedData);
}


// Handle Disconnection
void Server::onDisconnected() {
	// Access sender() within the Server class, which inherits from QObject
	const auto client = qobject_cast<QLocalSocket*>(sender());
	if (!client)
		return;
	clients.removeAll(client);
	client->deleteLater();
	qDebug() << "Client disconnected.";
}

// Retry Script Execution
void Server::retryScriptExecution(QLocalSocket* client, const QString& executionId, const QString& script, const QVariantList& arguments) {
	const auto future = pythonRunner->runScriptAsync(executionId, script, arguments);
	auto retryWatcher = new QFutureWatcher<PythonResult>(this);
	connect(retryWatcher, &QFutureWatcher<PythonResult>::finished, this,
		[this, retryWatcher, client, executionId, script, arguments]() {
			const auto retryResult = retryWatcher->future().result();
			QJsonObject responseObj;
			if (retryResult.isSuccess()) {
				responseObj["status"] = "success";
				responseObj["stdout"] = retryResult.getOutput();
				responseObj["stderr"] = retryResult.getErrorOutput();
				responseObj["executionTime"] = retryResult.getExecutionTime();
			
			}
			else {
				responseObj["status"] = "error";
				responseObj["stdout"] = retryResult.getOutput();
				responseObj["stderr"] = retryResult.getErrorOutput();
				responseObj["executionTime"] = retryResult.getExecutionTime();
			
			}
			responseObj["isScript"] = true;
			responseObj["executionId"] = executionId;
			sendResponse(client, responseObj);
			retryWatcher->deleteLater();
		});
	retryWatcher->setFuture(future);
}
void Server::handleInstallLocalPackageCommand(QLocalSocket* client, const QJsonObject& obj) {
	const auto packagePath = obj["packagePath"].toString();
	const auto executionId = obj["executionId"].toString();
	QJsonObject responseObj;

	if (packagePath.isEmpty()) {
		sendErrorResponse(client, "Package path is empty.", executionId);
		return;
	}

	if (executionId.isEmpty()) {
		sendErrorResponse(client, "Execution ID is empty.");
		return;
	}

	// Verify if the package path is valid
	QFileInfo packageInfo(packagePath);
	if (!packageInfo.exists() || !packageInfo.isDir()) {
		sendErrorResponse(client, QString("Package path does not exist or is not a directory: %1").arg(packagePath), executionId);
		return;
	}

	// Store the execution mapping
	ExecutionData execData;
	execData.client = client;
	execData.packagesToInstall.append(packagePath);
	execData.remainingPackages = 1; // Only one package
	execData.hasError = false;
	executionMap.insert(executionId, execData);

	// Initiate asynchronous installation of the local package
	pythonEnv->installLocalPackage(executionId, packagePath);

	// Send initial response indicating installation has started
	responseObj["status"] = "started";
	responseObj["message"] = QString("Installation of local package from '%1' started.").arg(packagePath);
	responseObj["executionId"] = executionId;
	responseObj["updateEvent"] = false;
	responseObj["isScript"] = false;

	sendResponse(client, responseObj);
}

void Server::onPackageOperationFinished(const QString& executionId, OperationType operation, const QString& identifier, const PythonResult& result) {
	QJsonObject responseObj;
	responseObj["executionId"] = executionId;

	if (result.isSuccess()) responseObj["status"] = "success";
	
	else responseObj["status"] = "error";
	
	// Optionally include other result details
	responseObj["stdout"] = result.getOutput();
	responseObj["stderr"] = result.getErrorOutput();
	responseObj["executionTime"] = result.getExecutionTime();
	responseObj["updateEvent"] = false;
	responseObj["isScript"] = false;

	for (const auto& client : clients) sendResponse(client, responseObj);
}

// Slot to handle package operation progress
void Server::onPackageOperationProgress(const QString& executionId, OperationType operation, const QString& identifier, const QString& progressMessage) {
	QJsonObject responseObj;

	switch (operation) {
	case OperationType::Install:
		responseObj["status"] = "installing";
		break;
	case OperationType::Reinstall:
		responseObj["status"] = "reinstalling";
		break;
	case OperationType::Update:
		responseObj["status"] = "updating";
		break;
	case OperationType::InstallLocal:
		responseObj["status"] = "installingLocal";
		break;
	case OperationType::UpdateLocal:
		responseObj["status"] = "updatingLocal";
		break;
	case OperationType::Uninstall:
		responseObj["status"] = "uninstalling";
		break;
	case OperationType::UpgradeAll:
		responseObj["status"] = "upgradingAll";
		break;
	case OperationType::Search:
		responseObj["status"] = "searching";
		break;
	default:
		responseObj["status"] = "processing";
		break;
	}

	responseObj["stage"] = progressMessage;
	responseObj["executionId"] = executionId;
	responseObj["updateEvent"] = true;
	responseObj["isScript"] = false;
	for (const auto& client : clients) sendResponse(client, responseObj);
}