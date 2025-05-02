#include "PythonRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <algorithm>


namespace            // helpers local to this TU
{
	inline constexpr QLatin1String kSentinel{ "__INIT_DONE__" };

	inline QByteArray wrap(QStringView src)
	{
		const QByteArray body = src.toUtf8().toHex();   // ASCII‑only
		return QByteArrayLiteral(
			"import textwrap,sys;exec(compile(bytes.fromhex('") +
			body +
			QByteArrayLiteral(
				"').decode(),'<init>','exec'))\n");
	}

/* ────────── helpers ─────────────────────────────────────────── */

} // namespace
QString PythonRunner::sitePackages(const QString& home)
{
#ifdef Q_OS_WIN
	return QDir(home).filePath("Lib/site-packages");
#else
	return QDir(home).filePath("lib/python3.11/site-packages");
#endif
}
/* static */ QString PythonRunner::detectPythonHome()
{
	QDir d(QCoreApplication::applicationDirPath());
	d.cd("python");
	return d.absolutePath();
}
/* static */ QString PythonRunner::detectPythonExe(const QString& home)
{
#ifdef Q_OS_WIN
	return QDir(home).filePath("python.exe");
#else
	return QDir(home).filePath("bin/python3");
#endif
}
/* static */ QProcessEnvironment PythonRunner::makeEnv(const QString& home)
{
	QProcessEnvironment e = QProcessEnvironment::systemEnvironment();
	e.remove("PYTHONHOME");
	e.remove("PYTHONPATH");

#if defined(Q_OS_WIN)
	constexpr QChar SEP = u';';
#else
	constexpr QChar SEP = u':';
#endif
	/* purge foreign pythons from PATH */
	auto parts = e.value("PATH").split(SEP, Qt::SkipEmptyParts);
	parts.erase(std::remove_if(parts.begin(), parts.end(),
		[](const QString& s)
		{ return s.contains("python", Qt::CaseInsensitive); }),
		parts.end());
#ifdef Q_OS_WIN
	const QString bin = QDir(home).filePath("Scripts");
#else
	const QString bin = QDir(home).filePath("bin");
#endif
	e.insert("PATH", bin + SEP + parts.join(SEP));
	e.insert("PYTHONHOME", home);
	e.insert("PYTHONPATH", sitePackages(home));

	const QString hp = QDir::homePath();
	e.insert("HOME", hp);
	e.insert("MPLCONFIGDIR", hp);
	e.insert("USERPROFILE", hp);
	e.insert("PYTHONUTF8", QStringLiteral("1"));
	return e;
}



/* ────────── ctor / dtor ─────────────────────────────────────── */
PythonRunner::PythonRunner(QObject* parent)
	: QObject(parent),
	m_pyHome(detectPythonHome()),
	m_pyExe(detectPythonExe(m_pyHome)),
	m_env(makeEnv(m_pyHome))

{
//    m_pool.reserve(kMaxTotal);
//    m_idle.reserve(kMaxTotal);
// 
//    for (int i = 0; i < kMaxIdle; ++i)
// 		createProcess();
}

PythonRunner::~PythonRunner()
{
	for (auto* ex : std::as_const(m_byId))
		cancel(ex->id);
	for (auto* p : m_pool) {
		p->kill();
		p->deleteLater();
	}
}

/* ────────── pool helpers ────────────────────────────────────── */
QProcess* PythonRunner::createProcess()
{
	auto* p = new QProcess(this);
	p->setProcessEnvironment(m_env);
	p->setProgram(m_pyExe);
	p->setWorkingDirectory(m_pyHome);
	p->setArguments({ "-u", "-q", "-i" });

	connect(p, &QProcess::readyReadStandardOutput, this, &PythonRunner::onStdout);
	connect(p, &QProcess::readyReadStandardError, this, &PythonRunner::onStderr);
	connect(p, &QProcess::finished, this, &PythonRunner::onFinished);
	connect(p, &QProcess::errorOccurred, this, &PythonRunner::onError);

	p->start();
	p->waitForStarted();

	/* one‑time bootstrap for this interpreter */
	if (!m_initPayload.isEmpty()) {
		p->write(m_initPayload);
		p->waitForReadyRead();
		p->readAllStandardOutput();  // flush  __INIT_DONE__
	}

	m_pool.push_back(p);
	m_idle.push_back(true);
	return p;
}

/* ────────── pool helpers ────────────────────────────────────── */
QProcess* PythonRunner::allocateProc()
{
	for (int i = 0, n = m_pool.size(); i < n; ++i)
		if (m_idle[i]) {
			m_idle[i] = false;
			return m_pool[i];
		}

	if (m_pool.size() < m_maxTotal) {         // grow elastically
		auto* p = createProcess();
		m_idle.back() = false;               // mark new proc busy
		return p;
	}
	return nullptr;                          // hard limit reached
}


void PythonRunner::recycleProc(QProcess* p)
{
	p->readAllStandardOutput();
	p->readAllStandardError();


	int idx = m_pool.indexOf(p);
	if (idx >= 0) {
		const bool dead = (p->state() == QProcess::NotRunning);
		m_idle[idx] = true;
	
		/* if process was killed or crashed, drop it from the pool */
		if (dead) {
			p->deleteLater();
			m_pool.remove(idx);
			m_idle.remove(idx);

			/* keep our baseline of kMaxIdle ready interpreters */
			if (m_pool.size() < m_maxIdle)
				createProcess();
		}
	}

	/* trim surplus idle interpreters */
	int idleCnt = std::count(m_idle.begin(), m_idle.end(), true);
	while (idleCnt > m_maxIdle) {
		int killIdx = m_idle.lastIndexOf(true);
		m_pool[killIdx]->kill();
		m_pool[killIdx]->deleteLater();
		m_pool.remove(killIdx);
		m_idle.remove(killIdx);
		--idleCnt;
	}

	/* run queued jobs, if any */
	if (!m_pending.isEmpty()) {
		const auto [nid, nscript, nto] = m_pending.dequeue();
		runScriptAsync(nid, nscript, {}, nto);
	}
}

// ── hex‑wrapper helper: compile‑first, exec‑second ─────────────────────────
// ── hex‑wrapper helper: compile‑first, exec‑second ────────────────
// ── hex‑wrapper helper: compile‑first, exec‑second ────────────────
QByteArray PythonRunner::wrapScript(QStringView script, QByteArrayView sentinel)
{
	static const QByteArray kOpener = QByteArrayLiteral(
		"src=bytes.fromhex('");
	static const QByteArray kMid1 = QByteArrayLiteral(
		"').decode()\n"
		"import traceback,sys\n"
		"sentinel='");
	static const QByteArray kMid2 = QByteArrayLiteral(
		"'\n"
		"try:\n"
		"    code=compile(src,'<stdin>','exec')\n"
		"    exec(code,globals())\n"
		"    print(sentinel)\n"              // success → stdout
		"except Exception:\n"
		"    traceback.print_exc(file=sys.stderr)\n"
		"    sys.stderr.write(sentinel+'\\n')\n\n"); // error → stderr

	const QByteArray body = script.toUtf8().toHex();   // ASCII‑only

	QByteArray payload;
	payload.reserve(kOpener.size() + body.size()
		+ kMid1.size() + sentinel.size()
		+ kMid2.size());

	payload += kOpener;
	payload += body;
	payload += kMid1;
	payload += sentinel;
	payload += kMid2;
	return payload;
}


/* ────────── public API ──────────────────────────────────────── */
QFuture<PythonResult>
PythonRunner::runScriptAsync(const QString& id,
	const QString& script,
	const QVariantList& /*unused*/,
	int timeoutMs)
{
	QProcess* proc = allocateProc();
	if (!proc) {
		m_pending.enqueue({ id, script, timeoutMs });
		QPromise<PythonResult> dummy; dummy.finish();
		return dummy.future();
	}

	QTimer* timer = nullptr;
	if (timeoutMs > 0) {
		timer = new QTimer(this);
		timer->setSingleShot(true);
		timer->setInterval(timeoutMs);
		connect(timer, &QTimer::timeout, this, &PythonRunner::onTimeout);
	}

	auto* ex = new Exec{ id, "__END_" + id + "__", proc, timer };
	ex->elapsed.start();
	m_byId.insert(id, ex);
	m_byProc.insert(proc, ex);
	if (timer) timer->start();



	proc->write(wrapScript(script, ex->sentinel.toUtf8()));


	++m_active;
	return ex->promise.future();
}

bool PythonRunner::cancel(const QString& id)
{
	if (auto* ex = m_byId.value(id, nullptr)) {
		ex->errBuf = "Script cancelled";
		ex->proc->kill();    // handled in onFinished/onError
		return true;
	}
	return false;
}


bool PythonRunner::init(QString const& payload,
	const int  maxIdle,
	const int  maxTotal)
{
	if (m_ready) return false;           // already initialised

	m_initPayload = payload.toUtf8();   // raw, *unwrapped* script
	m_initPayload = wrap(QString::fromUtf8(m_initPayload)); // hex‑wrap
	m_maxIdle = std::max(1, maxIdle);
	m_maxTotal = std::max(m_maxIdle, maxTotal);

	m_pool.reserve(m_maxTotal);
	m_idle.reserve(m_maxTotal);
	for (int i = 0; i < m_maxIdle; ++i)
		createProcess();

	m_ready = true;
	return true;
}

bool PythonRunner::onlyCRLF(const QByteArray& buf) const {

	return std::all_of(buf.cbegin(), buf.cend(), [](char c) { return c == '\r' || c == '\n'; });
}



/* ────────── slots ──────────────────────────────────────────── */
void PythonRunner::onStdout()
{
	auto* p = qobject_cast<QProcess*>(sender());
	auto* ex = execFor(p);
	if (!ex) return;


	auto chunk = p->readAllStandardOutput();

// 	static const QRegularExpression promptRx(QStringLiteral(R"((?m)^(>>> |\.\.\. ))"));
// 
// 	QString tmp = QString::fromUtf8(chunk);
// 	tmp.remove(promptRx);            // works on QString
// 	chunk = tmp.toUtf8();            // back to QByteArray
	
	//if (!ex->outBuf.isEmpty() && !onlyCRLF(ex->outBuf))
	ex->outBuf += chunk;


	const auto key = ex->sentinel.toUtf8();

	if (ex->outBuf.contains(key)) {
	
		chunk.replace(key, {});
		ex->outBuf.replace(key, {});
	
		if (!chunk.isEmpty() && !onlyCRLF(chunk))
			emit scriptOutput(ex->id, QString::fromUtf8(chunk));

		onFinished(0, QProcess::NormalExit);
		return;
	}

	if(!ex->failed)
		emit scriptOutput(ex->id, QString::fromUtf8(chunk));
	
	//ex->outBuf.clear();
}

void PythonRunner::onStderr()
{
	auto* p = qobject_cast<QProcess*>(sender());
	auto* ex = execFor(p);
	if (!ex) return;

	const auto chunk = p->readAllStandardError();

	QByteArray filtered;
	filtered.reserve(chunk.size());

	for (const auto& line : chunk.split('\n')) {
		const auto trimmed = QByteArray(line).trimmed();
		if (trimmed.startsWith(">>>") || trimmed.startsWith("..."))
			continue;                         // skip REPL prompts
		filtered += line;
		filtered += '\n';
	}

	ex->errBuf += filtered;
	const auto key = ex->sentinel.toUtf8();

	if (ex->errBuf.contains(key)) {

		ex->errBuf.replace(key, {});
		ex->failed = true;

		onFinished(0, QProcess::NormalExit);
		return;
	}
}

void PythonRunner::onFinished(int exitCode, QProcess::ExitStatus status)
{
	auto* p = qobject_cast<QProcess*>(sender());
	auto* ex = execFor(p);
	if (!ex) { recycleProc(p); return; }

	if (ex->timer) { ex->timer->stop(); ex->timer->deleteLater(); }

	const bool ok = !ex->failed && (status == QProcess::NormalExit && exitCode == 0);
	PythonResult res(ex->id, ok,
		QString::fromUtf8(ex->outBuf),
		QString::fromUtf8(ex->errBuf),
		ex->elapsed.elapsed());
	ex->promise.addResult(res);
	ex->promise.finish();
	emit scriptFinished(ex->id, res);

	m_byId.remove(ex->id);
	m_byProc.remove(p);
	--m_active;
	delete ex;
	recycleProc(p);
}
void PythonRunner::onError(QProcess::ProcessError)
{
	onFinished(-1, QProcess::CrashExit);
}
void PythonRunner::onTimeout()
{
	auto* t = qobject_cast<QTimer*>(sender());
	for (auto it = m_byId.begin(); it != m_byId.end(); ++it)
		if (it.value()->timer == t) { it.value()->proc->kill(); return; }
}
