#include "encryption.h"
#include <cryptopp/cryptlib.h>
#include <cryptopp/aes.h>
#include <cryptopp/osrng.h>
#include <cryptopp/modes.h>
#include <cryptopp/filters.h>
#include <string>
#include <vector>
#include <QCryptographicHash>
#include <QSysInfo>
#include <QDebug>

QByteArray Encryption::encryptData(const QByteArray& plainData, QByteArray& iv)
{
	const auto key = generateSecretKey();

	CryptoPP::AutoSeededRandomPool rng;
	iv.resize(CryptoPP::AES::BLOCKSIZE);
	rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(iv.data()), iv.size());

	QByteArray encrypted;
	try {
		// Initialize the encryptor with the key and IV
		CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption encryptor(
			reinterpret_cast<const CryptoPP::byte*>(key.constData()), key.size(),
			reinterpret_cast<const CryptoPP::byte*>(iv.constData()));

		// Temporary std::string to store encrypted data
		std::string encryptedString;

		// Perform encryption
		CryptoPP::StringSource ss(
			reinterpret_cast<const CryptoPP::byte*>(plainData.constData()), plainData.size(),
			true, // Pump all data
			new CryptoPP::StreamTransformationFilter(
				encryptor,
				new CryptoPP::StringSink(encryptedString)
			)
		);

		// Convert the encrypted std::string to QByteArray
		encrypted = QByteArray::fromStdString(encryptedString);
	}
	catch (const CryptoPP::Exception& e) {
		qWarning() << "Encryption error:" << e.what();
	}
	return encrypted;
}

QByteArray Encryption::decryptData(const QByteArray& encryptedData,  const QByteArray& iv)
{
	const auto key = generateSecretKey();
	QByteArray decrypted;
	try {
		// Initialize the decryptor with the key and IV
		CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption decryptor(
			reinterpret_cast<const CryptoPP::byte*>(key.constData()), key.size(),
			reinterpret_cast<const CryptoPP::byte*>(iv.constData()));

		// Temporary std::string to store decrypted data
		std::string decryptedString;

		// Perform decryption
		CryptoPP::StringSource ss(
			reinterpret_cast<const CryptoPP::byte*>(encryptedData.constData()), encryptedData.size(),
			true, // Pump all data
			new CryptoPP::StreamTransformationFilter(
				decryptor,
				new CryptoPP::StringSink(decryptedString)
			)
		);

		// Convert the decrypted std::string to QByteArray
		decrypted = QByteArray::fromStdString(decryptedString);
	}
	catch (const CryptoPP::Exception& e) {
		qWarning() << "Decryption error:" << e.what();
	}
	return decrypted;
}

QByteArray Encryption::generateSecretKey()
{
	// Combine hardware-specific details
	QByteArray hardwareInfo;
	hardwareInfo.append(QSysInfo::machineHostName().toUtf8());
	hardwareInfo.append(QSysInfo::bootUniqueId());

	// Add a random salt for uniqueness
	hardwareInfo.append(staticSalt());

	// Hash the hardware info with the salt to create a 32-byte secret key
	QByteArray hashedKey = QCryptographicHash::hash(hardwareInfo, QCryptographicHash::Sha256);
	return hashedKey;
}

QByteArray Encryption::staticSalt()
{
	return QCryptographicHash::hash("74d83579f8gzpbhu9n", QCryptographicHash::Sha256); // Replace with a strong static salt
}

QString Encryption::generateServerName()
{
	QByteArray hardwareInfo;
	hardwareInfo.append(QSysInfo::machineHostName().toUtf8());
	hardwareInfo.append(QSysInfo::prettyProductName().toUtf8());

	// Add a salt to the server name
	QByteArray salt = hardwareInfo.append(staticSalt());
	salt.append("sad");

	// Hash the hardware info with the salt for a unique server name
	QByteArray hashedName = QCryptographicHash::hash(hardwareInfo, QCryptographicHash::Sha256);
	return hashedName.toHex();
}
