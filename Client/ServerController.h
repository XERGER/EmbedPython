// ServerController.h
#ifndef SERVERCONTROLLER_H
#define SERVERCONTROLLER_H
#include "global.h"

#include <QObject>
#include <QProcess>
#include <QTimer>

class PYTHHONCLIENT_EXPORT ServerController : public QObject {
	Q_OBJECT

public:
	explicit ServerController(QString const& enginePath, QObject* parent = nullptr);
	~ServerController();

	void startServer();
	void stopServer();
	bool isServerRunning() const;

signals:
	void serverStarted();
	void serverStopped();
	void serverCrashed();
	void hashMismatch();

private slots: // Ensure this section exists
	void handleServerCrash(); // Add this declaration if missing

	void restartServer();

private:
	void killExistingServers();
	bool verifyHash(const QString& filePath, const QByteArray& expectedHash);

	QProcess serverProcess;
	QTimer restartTimer;
	QString serverExecutablePath;
	QByteArray expectedHash;
};

#endif // SERVERCONTROLLER_H
