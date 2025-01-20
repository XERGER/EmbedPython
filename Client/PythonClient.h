#pragma once

#include "global.h"
#include <QObject>
#include <QLocalSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QTimer>
#include "PythonResult.h"

/**
 * @brief The PythonClient class provides a client interface for communicating with a local server
 * using QLocalSocket. It supports commands for installing packages, running scripts, and checking script syntax.
 */
class PYTHHONCLIENT_EXPORT PythonClient : public QObject {
    Q_OBJECT
public:
    explicit PythonClient(QObject* parent = nullptr);

    /**
     * @brief Connects to the local server.
     * @return True if connected successfully, false otherwise.
     */
    bool connectToServer();

    /**
     * @brief Sends a command to install a package.
     * @param package The name of the package to install.
     */
    void installPackage(const QString& executionId, const QString& package);

    /**
     * @brief Sends a command to install a local package.
     * @param packagePath The file path to the local package.
     */
    void installLocalPackage(const QString& executionId, const QString& packagePath);

    /**
     * @brief Sends a command to run a Python script on the server.
     * @param executionId Unique identifier for the script execution.
     * @param script The Python script to execute.
     * @param arguments A list of arguments for the script.
     * @param timeout The timeout for the script execution in milliseconds.
     */
    void runScript(const QString& executionId, const QString& script, const QVariantList& arguments, int timeout = 5000);

    /**
     * @brief Sends a command to check the syntax of a Python script.
     * @param executionId Unique identifier for the syntax check operation.
     * @param script The Python script to check for syntax errors.
     */
    void checkSyntax(const QString& executionId, const QString& script);

    /**
     * @brief Waits for the server to be ready before connecting.
     * @return True if the server becomes ready within the timeout, false otherwise.
     */
    bool waitForServerReady();

	void reinstallPackage(const QString& executionId, const QString& package);
	void updatePackage(const QString& executionId, const QString& package);
	void uninstallPackage(const QString& executionId, const QString& package);

	// New package management methods
	bool isPackageInstalled(const QString& executionId, const QString& package);
	QString getPackageVersion(const QString& package);
	QJsonObject getPackageInfo(const QString& package);

	void upgradeAllPackages();
	QStringList searchPackage(const QString& query);
	QStringList listInstalledPackages();

signals:
    /**
     * @brief Emitted when the client successfully connects to the server.
     */
    void connectedToServer();

	void scriptExecutionFinished(const PythonResult& result);
	void packageOperationFinished(const PythonResult& result);
	void packageOperationProgress(OperationType operation, const QString& progressMessage, const QString& executionId);

    /**
     * @brief Emitted when the client disconnects from the server.
     */
    void disconnectedFromServer();

private slots:
    /**
     * @brief Handles the socket's connected state.
     */
    void onConnected();

    /**
     * @brief Handles incoming data from the server.
     */
    void onReadyRead();

    /**
     * @brief Handles the socket's disconnected state.
     */
    void onDisconnected();

    /**
     * @brief Attempts to reconnect to the server.
     */
    void attemptReconnect();

private:
    QLocalSocket* socket;       ///< The local socket used for communication with the server.
    QTimer* reconnectTimer;     ///< Timer for retrying connection attempts.
	QByteArray buffer; // Buffer to store incomplete data

    /**
     * @brief Encrypts and sends a command to the server.
     * @param command The JSON object representing the command.
     */
    void sendCommand(const QJsonObject& command);

    /**
     * @brief Serializes a QVariantList to a QJsonArray.
     * @param arguments The list of arguments to serialize.
     * @return The serialized JSON array.
     */
    QJsonArray serializeVariantList(const QVariantList& arguments);
};