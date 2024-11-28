// server.h

#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QList>
#include <QFutureWatcher>
#include <memory>

// Include PythonResult directly since it's now fully defined
#include "Library/PythonResult.h"

// Forward declarations for Python integration
class PythonEnvironment;
class PythonRunner;

class Server : public QObject {
	Q_OBJECT
public:
	explicit Server(QObject* parent = nullptr);
	~Server();

	void startServer();

private slots:
	void onNewConnection();
	void onReadyRead();
	void onDisconnected();

private:
	// Command processing methods
	void processCommand(QLocalSocket* client, const QByteArray& data);
	void handleCheckSyntaxCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleExecuteCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleInstallPackageCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleInstallLocalPackageCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleInstallOpenAPIClientCommand(QLocalSocket* client);

	// Helper methods for script execution
	void handleScriptExecutionResult(QFutureWatcher<PythonResult>* watcher, QLocalSocket* client, const QString& executionId, const QString& script, QVariantList const& arguments );
	void handleMissingModules(QLocalSocket* client, QFutureWatcher<PythonResult>* watcher, const QString& executionId, const QString& script, QVariantList const& arguments,  const PythonResult& result);
	void retryScriptExecution(QLocalSocket* client, QFutureWatcher<PythonResult>* watcher, const QString& executionId, const QString& script, QVariantList const& arguments );

	// Utility methods
	QVariantList deserializeVariantList(const QJsonArray& array);
	QByteArray encryptResponse(const QJsonObject& responseObj);
	void sendErrorResponse(QLocalSocket* client, const QString& message, const QString& executionId = QString());
	void sendResponse(QLocalSocket* client, const QJsonObject& responseObj);

	// Member variables
	QLocalServer* localServer;
	QList<QLocalSocket*> clients;

	// Python Integration
	std::shared_ptr<PythonEnvironment> pythonEnv;
	std::unique_ptr<PythonRunner> pythonRunner;
};

#endif // SERVER_H
