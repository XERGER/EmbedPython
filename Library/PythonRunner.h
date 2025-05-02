#pragma once

#include "global.h"
#include "PythonResult.h"

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QFuture>
#include <QPromise>
#include <QHash>
#include <QProcessEnvironment>
#include <QElapsedTimer>
#include <QQueue>
#include <QVector>
#include <thread>

class LIBRARY_EXPORT PythonRunner final : public QObject
{
	Q_OBJECT
public:
	explicit PythonRunner(QObject* parent = nullptr);
	~PythonRunner() override;


	QFuture<PythonResult> runScriptAsync(const QString& id,
		const QString& script,
		const QVariantList& args = {},
		int timeoutMs = -1);

	bool cancel(const QString& id);

	bool init(QString const& payload,
		int        maxIdle = 3,
		int        maxTotal = 10);

signals:
	void scriptFinished(const QString& id, const PythonResult& result);
	void scriptOutput(const QString& id, const QString& chunk);

private slots:
	void onStdout();
	void onStderr();
	void onFinished(int, QProcess::ExitStatus);
	void onError(QProcess::ProcessError);
	void onTimeout();

private:
	static QString sitePackages(const QString& home);
	/* helpers */
	static QString detectPythonHome();
	static QString detectPythonExe(const QString& home);
	static QProcessEnvironment makeEnv(const QString& home);

	bool onlyCRLF(const QByteArray& buf) const;
	QProcess* allocateProc();        // idle or nullptr
	void      recycleProc(QProcess*);
	static QByteArray wrapScript(QStringView src, QByteArrayView sntl);
	QProcess* createProcess();
	struct Exec {
		QString              id;
		QString              sentinel;
		QProcess* proc{};
		QTimer* timer{};
		QPromise<PythonResult> promise;
		QElapsedTimer        elapsed;
		QByteArray           outBuf;
		QByteArray           errBuf;
		bool failed = false;
	};
	Exec* execFor(QProcess* p) const { return m_byProc.value(p, nullptr); }

	/* state */
	const QString             m_pyHome;
	const QString             m_pyExe;
	const QProcessEnvironment m_env;

	QVector<QProcess*> m_pool;
	QVector<bool>      m_idle;

	int                m_active{ 0 };

	QHash<QString, Exec*>    m_byId;
	QHash<QProcess*, Exec*>  m_byProc;
	QQueue<std::tuple<QString, QString, int>> m_pending;   // id, script, timeout


	QByteArray        m_initPayload;
	int               m_maxIdle = 3;
	int               m_maxTotal = 10;
	bool              m_ready = false;
};
