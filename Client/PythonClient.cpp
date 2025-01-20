#include "PythonClient.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QDebug>
#include <QThread>
#include "Encryption.h" // Assuming EncryptionUtil is defined as per the server-side


PythonClient::PythonClient(QObject* parent)
	: QObject(parent), socket(new QLocalSocket(this)), reconnectTimer(new QTimer(this)) {
	connect(socket, &QLocalSocket::connected, this, &PythonClient::onConnected);
	connect(socket, &QLocalSocket::readyRead, this, &PythonClient::onReadyRead);
	connect(socket, &QLocalSocket::disconnected, this, &PythonClient::onDisconnected);

// 	// Setup reconnect timer
// 	reconnectTimer->setInterval(5000); // Retry every 5 seconds
// 	reconnectTimer->setSingleShot(false);
// 	connect(reconnectTimer, &QTimer::timeout, this, &PythonClient::attemptReconnect);
}

bool PythonClient::connectToServer() {
	if (socket->state() == QLocalSocket::ConnectedState) {
		qDebug() << "Already connected to the server.";
		return true;
	}

	socket->connectToServer(Encryption::generateServerName());
	if (!socket->waitForConnected(5000)) { // Add connection timeout for robustness
		qWarning() << "Failed to connect to server within timeout.";
		return false;
	}
	return true;
}

void PythonClient::installPackage(const QString& executionId, const QString& package) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return;
	}
	QJsonObject command;
	command["executionId"] = executionId;
	command["command"] = "installPackage";
	command["package"] = package;
	sendCommand(command);
}

void PythonClient::installLocalPackage(const QString& executionId, const QString& packagePath) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";

		return;
	}
	QJsonObject command;
	command["executionId"] = executionId;
	command["command"] = "installLocalPackage";
	command["packagePath"] = packagePath;

	sendCommand(command);
}

void PythonClient::runScript(const QString& executionId, const QString& script, const QVariantList& arguments, int timeout) {
	if (!socket->isOpen()) {
		attemptReconnect();
		qWarning() << "Socket is not connected to the server.";
		return;
	}
	QJsonObject command;
	command["command"] = "execute";
	command["script"] = script;
	command["arguments"] = QJsonArray::fromVariantList(arguments);
	command["timeout"] = timeout;
	command["executionId"] = executionId; // Include the unique ID
	sendCommand(command);
}

void PythonClient::checkSyntax(const QString& executionId, const QString& script) {
	if (script.isEmpty()) {
		qWarning() << "Cannot check syntax: script is empty.";
		return;
	}

	QJsonObject command;
	command["command"] = "checkSyntax";
	command["executionId"] = executionId;
	command["script"] = script;

	sendCommand(command);
}

bool PythonClient::waitForServerReady() {

	qDebug() << "Waiting for server to be ready...(20 seconds)";
	for (int i = 0; i < 4; ++i) { // Retry for up to 20 seconds
		if (connectToServer())
			return true;
		QThread::sleep(1);
	}

	qWarning() << "Server is not ready after 20 seconds.";
	return false;
}

QJsonArray PythonClient::serializeVariantList(const QVariantList& arguments) {
	QJsonArray array;
	for (const QVariant& arg : arguments) {
		array.append(QJsonValue::fromVariant(arg)); // Store only the value
	}
	return array;
}
OperationType getOperationType(const QString& status) {
	static const QMap<QString, OperationType> statusMap = {
		{"installing", OperationType::Install},
		{"reinstalling", OperationType::Reinstall},
		{"updating", OperationType::Update},
		{"installingLocal", OperationType::InstallLocal},
		{"updatingLocal", OperationType::UpdateLocal},
		{"uninstalling", OperationType::Uninstall},
		{"upgradingAll", OperationType::UpgradeAll},
		{"searching", OperationType::Search},
		// Add other mappings as needed
	};

	return statusMap.value(status, OperationType::Search); // Default to Search or an appropriate default
}
void PythonClient::onConnected() {
	qDebug() << "Connected to server.";
	emit connectedToServer();
}

void PythonClient::onReadyRead() {
	buffer.append(socket->readAll());
	while (true) {
		if (buffer.size() < 4) {
			break;
		}
		QDataStream stream(buffer.left(4));
		stream.setByteOrder(QDataStream::BigEndian);
		quint32 messageLength;
		stream >> messageLength;
		if (messageLength == 0 || messageLength > (100 * 1024 * 1024)) {
			qWarning() << "Invalid message length:" << messageLength;
			socket->disconnectFromServer();
			buffer.clear();
			break;
		}
		if (buffer.size() < 4 + messageLength) {
			break;
		}
		QByteArray encryptedData = buffer.mid(4, messageLength);
		buffer.remove(0, 4 + messageLength);
		const int ivSize = 16;
		if (encryptedData.size() < ivSize) {
			qWarning() << "Encrypted data is too short to contain IV and ciphertext.";
			continue;
		}
		QByteArray iv = encryptedData.left(ivSize);
		QByteArray cipherText = encryptedData.mid(ivSize);
		QByteArray plainData = Encryption::decryptData(cipherText, iv);
		if (plainData.isEmpty()) {
			qWarning() << "Decryption failed.";
			continue;
		}
		QJsonParseError parseError;
		QJsonDocument doc = QJsonDocument::fromJson(plainData, &parseError);
		if (doc.isNull()) {
			qWarning() << "JSON parse error:" << parseError.errorString();
			qDebug() << "Received plain data:" << plainData;
			continue;
		}
		if (!doc.isObject()) {
			qWarning() << "Received JSON is not an object.";
			qDebug() << "Received JSON:" << doc;
			continue;
		}
		QJsonObject response = doc.object();
		qDebug() << "Received response from server:" << response;
		QString status = response.value("status").toString();
		QString executionId = response.value("executionId").toString();
		bool isScript = response.value("isScript").toBool(false);
		bool updateEvent = response.value("updateEvent").toBool(false);
		if (updateEvent) {
			QString stage = response.value("stage").toString();
			OperationType operation = getOperationType(status);
			emit packageOperationProgress(operation, stage, executionId);
			continue;
		}
		if (isScript) {
			PythonResult result;
			result = PythonResult(
				executionId,
				status == "success",
				response.value("stdout").toString(),
				response.value("stderr").toString(),
				response.value("executionTime").toDouble()
			);
			if (response.contains("errorCode")) {
				result.setErrorCode(response.value("errorCode").toInt());
			}
			emit scriptExecutionFinished(result);
			continue;
		}
		if (!isScript) {
			if (status == "success" || status == "error" || status == "cancelled") {
				PythonResult result;
				result = PythonResult(
					executionId,
					status == "success",
					response.value("stdout").toString(),
					response.value("stderr").toString(),
					response.value("executionTime").toDouble()
				);
				if (response.contains("errorCode")) {
					result.setErrorCode(response.value("errorCode").toInt());
				}
				emit packageOperationFinished(result);
				continue;
			}
			qDebug() << "Received intermediate status:" << status;
		}
	}
}



void PythonClient::attemptReconnect() {
	if (socket->state() == QLocalSocket::ConnectedState) {
		qDebug() << "Already connected. Reconnect attempt skipped.";
		reconnectTimer->stop();
		return;
	}

	qDebug() << "Attempting to reconnect to server...";
	if (connectToServer()) {
		qDebug() << "Reconnected successfully.";
		reconnectTimer->stop();
	}
	else {
		qDebug() << "Reconnect attempt failed. Will retry.";
	}
}

void PythonClient::onDisconnected() {

	qWarning() << "Disconnected from server.";
	emit disconnectedFromServer();

	// Start the reconnect timer if disconnected
	if (!reconnectTimer->isActive()) {
		reconnectTimer->start();
	}
}

void PythonClient::sendCommand(const QJsonObject& command) {
	if (socket->state() != QLocalSocket::ConnectedState) {
		qWarning() << "Cannot send command. Not connected to server.";
		return;
	}

	QJsonDocument doc(command);
	QByteArray plainData = doc.toJson(QJsonDocument::Compact);

	// Encrypt the data
	QByteArray iv;
	QByteArray cipherText = Encryption::encryptData(plainData, iv);
	if (cipherText.isEmpty()) {
		qWarning() << "Encryption failed.";
		return;
	}

	QByteArray packet = iv + cipherText; // Combine IV and encrypted data

	// Prepend the 4-byte length prefix
	quint32 packetLength = packet.size();
	QByteArray lengthPrefix;
	QDataStream lengthStream(&lengthPrefix, QIODevice::WriteOnly);
	lengthStream.setByteOrder(QDataStream::BigEndian);
	lengthStream << packetLength;

	QByteArray finalPacket = lengthPrefix + packet;

	// Write to the socket
	socket->write(finalPacket);
	socket->flush(); // Ensure data is sent immediately
}


// New package management methods
bool PythonClient::isPackageInstalled(const QString& executionId, const QString& package) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return false;
	}
	QJsonObject command;
	command["command"] = "isPackageInstalled";
	command["package"] = package;
	command["executionId"] = executionId;
	sendCommand(command);
	// The result will be emitted via packageOperationFinished signal
	return true;
}

QString PythonClient::getPackageVersion(const QString& package) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return QString();
	}
	QJsonObject command;
	command["command"] = "getPackageVersion";
	command["package"] = package;
	QString executionId = QUuid::createUuid().toString();
	command["executionId"] = executionId;
	sendCommand(command);
	// The result will be emitted via packageOperationFinished signal
	return QString();
}

QJsonObject PythonClient::getPackageInfo(const QString& package) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return QJsonObject();
	}
	QJsonObject command;
	command["command"] = "getPackageInfo";
	command["package"] = package;
	QString executionId = QUuid::createUuid().toString();
	command["executionId"] = executionId;
	sendCommand(command);
	// The result will be emitted via packageOperationFinished signal
	return QJsonObject();
}

void PythonClient::reinstallPackage(const QString& executionId, const QString& package) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return;
	}
	QJsonObject command;
	command["command"] = "reinstallPackage";
	command["package"] = package;
	command["executionId"] = executionId;

	sendCommand(command);
}

void PythonClient::updatePackage(const QString& executionId, const QString& package) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return;
	}
	QJsonObject command;
	command["command"] = "updatePackage";
	command["package"] = package;
	command["executionId"] = executionId;
	sendCommand(command);
}

void PythonClient::uninstallPackage(const QString& executionId, const QString& package) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return;
	}
	QJsonObject command;
	command["command"] = "uninstallPackage";
	command["package"] = package;
	command["executionId"] = executionId;
	sendCommand(command);
}

void PythonClient::upgradeAllPackages() {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return;
	}
	QJsonObject command;
	command["command"] = "upgradeAllPackages";
	QString executionId = QUuid::createUuid().toString();
	command["executionId"] = executionId;
	sendCommand(command);
}

QStringList PythonClient::searchPackage(const QString& query) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return QStringList();
	}
	QJsonObject command;
	command["command"] = "searchPackage";
	command["query"] = query;
	QString executionId = QUuid::createUuid().toString();
	command["executionId"] = executionId;
	sendCommand(command);
	// The result will be emitted via packageOperationFinished signal
	return QStringList();
}

QStringList PythonClient::listInstalledPackages() {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return QStringList();
	}
	QJsonObject command;
	command["command"] = "listInstalledPackages";
	QString executionId = QUuid::createUuid().toString();
	command["executionId"] = executionId;
	sendCommand(command);
	// The result will be emitted via packageOperationFinished signal
	return QStringList();
}
