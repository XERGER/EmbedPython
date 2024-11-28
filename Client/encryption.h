#pragma once
#include <QString>
#include <QByteArray>
#include "global.h"

class PYTHHONCLIENT_EXPORT Encryption {

public:
	static QByteArray encryptData(const QByteArray& plainData, QByteArray& iv);
	static QByteArray decryptData(const QByteArray& encryptedData, const QByteArray& iv);

	static QString generateServerName();

private:
	static QByteArray generateSecretKey();
	static QByteArray staticSalt();
	static QByteArray generatebaseSecret();
};
