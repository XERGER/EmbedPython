// main.cpp
#include <QCoreApplication>
#include "server.h"
#include <QFile>
#include <QMutex>
#include <QMutexLocker>

QFile logFile;
QMutex logMutex;

void recreateLogFile() {
	QString logFilePath = QCoreApplication::applicationDirPath() + "/engine.log"; // Use current app directory
	logFile.setFileName(logFilePath);
	if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		qWarning() << "Unable to recreate log file.";
		return;
	}
	logFile.close(); // Close it right after clearing the contents
}

// Custom message handler for logging
void customMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
	// Lock to avoid race conditions when multiple threads write to the log
	QMutexLocker locker(&logMutex);

	// Open the log file if it's not already open
	if (!logFile.isOpen()) {
		QString logFilePath = QCoreApplication::applicationDirPath() + "/engine.log"; // Use current app directory
		logFile.setFileName(logFilePath);
		if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
			return;  // If file can't be opened, return
		}
	}

	// Create a text stream to write to the file
	QTextStream out(&logFile);
	out.setEncoding(QStringConverter::Utf8);
	QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

	// Based on message type, write different prefixes
	QString logLine;
	switch (type) {
	case QtDebugMsg:
		logLine = QString("%1 [DEBUG]: %2").arg(timeStamp, msg);
		out << logLine << Qt::endl;
		qDebug() << logLine; // Print to console in debug mode
		break;
	case QtWarningMsg:
		logLine = QString("%1 [WARNING]: %2").arg(timeStamp, msg);
		out << logLine << Qt::endl;
		qWarning() << logLine; // Print to console
		break;
	case QtCriticalMsg:
		logLine = QString("%1 [CRITICAL]: %2").arg(timeStamp, msg);
		out << logLine << Qt::endl;
		qCritical() << logLine; // Print to console
		break;
	case QtFatalMsg:
		logLine = QString("%1 [FATAL]: %2").arg(timeStamp, msg);
		out << logLine << Qt::endl;
		qFatal("%s", logLine.toUtf8().constData()); // Print to console and abort
		break;
	}

	// Flush the stream to ensure immediate write
	out.flush();
}

int main(int argc, char* argv[])
{
	QCoreApplication a(argc, argv);



	recreateLogFile();


	qInstallMessageHandler(customMessageHandler);




	Server server;
	server.startServer();

	int ret = a.exec();


	return ret;
}
