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

	// Setup reconnect timer
	reconnectTimer->setInterval(5000); // Retry every 5 seconds
	reconnectTimer->setSingleShot(false);
	connect(reconnectTimer, &QTimer::timeout, this, &PythonClient::attemptReconnect);
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

void PythonClient::installPackage(const QString& package) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return;
	}
	QJsonObject command;
	command["command"] = "installPackage";
	command["package"] = package;
	sendCommand(command);
}

void PythonClient::installLocalPackage(const QString& packagePath) {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return;
	}
	QJsonObject command;
	command["command"] = "installLocalPackage";
	command["packagePath"] = packagePath;
	sendCommand(command);
}

void PythonClient::installOpenAPIClient() {
	if (!socket->isOpen()) {
		qWarning() << "Socket is not connected to the server.";
		return;
	}
	QJsonObject command;
	command["command"] = "installOpenAPIClient";
	sendCommand(command);
}

void PythonClient::runScript(const QString& executionId, const QString& script, const QVariantList& arguments, int timeout) {
	if (!socket->isOpen()) {
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
	const QString serverName = Encryption::generateServerName();
	qDebug() << "Waiting for server to be ready...(20 seconds)";

	for (int i = 0; i < 20; ++i) { // Retry for up to 20 seconds
		QThread::sleep(1);
		QLocalSocket testSocket;
		testSocket.connectToServer(serverName);
		if (testSocket.waitForConnected(2000)) { // 2 seconds per attempt
			testSocket.disconnectFromServer();
			return true;
		}
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

void PythonClient::onConnected() {
	qDebug() << "Connected to server.";
	emit connectedToServer();
}

void PythonClient::onReadyRead() {
	QByteArray encryptedData = socket->readAll();

	// Decrypt the incoming data
	QByteArray iv = encryptedData.left(16); // Extract the IV
	QByteArray cipherText = encryptedData.mid(16);
	QByteArray plainData = Encryption::decryptData(cipherText, iv);

	if (plainData.isEmpty()) {
		qWarning() << "Decryption failed.";
		return;
	}

	QJsonDocument doc = QJsonDocument::fromJson(plainData);
	if (doc.isObject()) {
		QJsonObject response = doc.object();
		emit dataReceived(response);
		qDebug() << "Received response from server:" << response;
	}
	else {
		qWarning() << "Received invalid JSON response.";
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

	QByteArray packet = iv + cipherText; // Prepend the IV
	socket->write(packet);
	socket->flush(); // Ensure data is sent immediately
}
