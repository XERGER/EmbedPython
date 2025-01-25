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
#include <QHash>
#include <QFutureWatcher>
#include <memory>

// Include PythonResult directly since it's now fully defined
#include "Library/PythonResult.h"
#include "Library/PythonSyntaxCheck.h"

// Forward declarations for Python integration
class PythonEnvironment;
class PythonRunner;

// ExecutionData struct definition
struct ExecutionData {
	QLocalSocket* client;
	QString script;
	QVariantList arguments;
	QStringList packagesToInstall; // List of packages to be installed or managed
	int remainingPackages;
	bool hasError;

	ExecutionData(QLocalSocket* c = nullptr, const QString& scr = "", const QVariantList& args = {}, const QStringList& pkgs = {})
		: client(c), script(scr), arguments(args), packagesToInstall(pkgs), remainingPackages(pkgs.size()), hasError(false) {}
};

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

	void onPackageOperationProgress(const QString& executionId, OperationType operation, const QString& identifier, const QString& progressMessage);
	void onPackageOperationFinished(const QString& executionId, OperationType operation, const QString& identifier, const PythonResult& result);

	void onSyntaxCheckFinished(const QString& executionId, const PythonResult& result);

private:
	// Command processing methods
	void processCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleListInstalledPackagesCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleGetPackageInfoCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleUpgradeAllPackagesCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleSearchPackageCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleCheckSyntaxCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleCancelCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleExecuteCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleInstallPackageCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleUninstallPackageCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleReinstallPackageCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleUpdatePackageCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleInstallLocalPackageCommand(QLocalSocket* client, const QJsonObject& obj);
	void handleUpdateLocalPackageCommand(QLocalSocket* client, const QJsonObject& obj);

	// Helper methods for script execution
	void handleScriptExecutionResult(QFutureWatcher<PythonResult>* watcher, QLocalSocket* client, const QString& executionId, const QString& script, QVariantList const& arguments);
	//void handleMissingModules(QLocalSocket* client, QFutureWatcher<PythonResult>* watcher, const QString& executionId, const QString& script, const QVariantList& arguments, const PythonResult& result);
	void retryScriptExecution(QLocalSocket* client, const QString& executionId, const QString& script, const QVariantList& arguments);

	// Utility methods
	QVariantList deserializeVariantList(const QJsonArray& array);
	QByteArray encryptResponse(const QJsonObject& responseObj);
	void sendErrorResponse(QLocalSocket* client, const QString& message, const QString& executionId = QString());
	void sendResponse(QLocalSocket* client, const QJsonObject& responseObj);

	// Member variables
	QLocalServer* localServer;
	QList<QLocalSocket*> clients;
	QMap<QLocalSocket*, QByteArray> bufferMap;
	// Python Integration
	std::shared_ptr<PythonEnvironment> pythonEnv;
	std::unique_ptr<PythonRunner> pythonRunner;
	std::unique_ptr<PythonSyntaxCheck> syntaxChecker;

	// Mapping executionId to relevant data
	QHash<QString, ExecutionData> executionMap;
};

#endif // SERVER_H
