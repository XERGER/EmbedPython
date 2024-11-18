// PythonVirtualEnvTest.cpp
#include "../pch.h"
#include "Library/PythonVirtualEnv.h"
#include <gtest/gtest.h>
#include <QDir>
#include <QCoreApplication>
#include "Library/PythonEnvironment.h"

class PythonVirtualEnvTest : public ::testing::Test {
protected:
	void SetUp() override {
		tempDir = QDir::tempPath() + "/test_env";
		// Ensure the directory is clean
		QDir dir(tempDir);
		if (dir.exists()) {
			dir.removeRecursively();
		}
		virtualEnv = new PythonVirtualEnv(tempDir);
	}

	void TearDown() override {
		delete virtualEnv;
		QDir dir(tempDir);
		dir.removeRecursively();
	}
	QString obfuscateHash(const QString& hash, const QString& salt) const {
		QByteArray hashBytes = QByteArray::fromHex(hash.toUtf8());
		QByteArray saltBytes = salt.toUtf8();
		QByteArray obfuscatedBytes;

		for (int i = 0; i < hashBytes.size(); ++i) {
			obfuscatedBytes.append(hashBytes[i] ^ saltBytes[i % saltBytes.size()]);
		}

		return obfuscatedBytes.toHex();
	}
	QString tempDir;
	PythonVirtualEnv* virtualEnv;
};

inline std::ostream& operator<<(std::ostream& os, const QString& qstr) {
	os << qstr.toStdString();
	return os;
}

TEST_F(PythonVirtualEnvTest, VerifyPythonExecutableHash) {
	// Initialize PythonEnvironment
	PythonEnvironment env;
	ASSERT_TRUE(env.init()) << QString("Failed to initialize PythonEnvironment.");

	// Compute the hash of the Python executable
	QString pythonExePath;
#ifdef Q_OS_WIN
	pythonExePath = QDir(env.getDefaultEnvPath()).filePath("python.exe");
#else
	pythonExePath = QDir(env.getDefaultEnvPath()).filePath("bin/python3");
#endif

	// Ensure the Python executable exists
	QFileInfo checkFile(pythonExePath);
	ASSERT_TRUE(checkFile.exists() && checkFile.isFile()) << pythonExePath;


	// Compute the SHA-256 hash
	QString currentHash = env.computeFileHash(pythonExePath);
	ASSERT_FALSE(currentHash.isEmpty()) << QString("Failed to compute hash of Python executable.");
	

	// Verify the hash using the existing method
	bool isValid = env.verifyPythonExecutable();
	if (!isValid) {
		// Define the salt used in deobfuscateExpectedHash()
		const QString salt = "s0m3S@ltV@lu3";

		// Obfuscate the current hash
		QString newObfuscatedHash = obfuscateHash(currentHash, salt);

		// Log results with Qt
		qWarning() << "Hash mismatch detected!";
		qWarning() << "Current Python Executable Hash (SHA-256):" << currentHash;
		qWarning() << "New Obfuscated Hash to Update in Code:" << newObfuscatedHash;

		// Report failure
		ASSERT_TRUE(isValid) << QString("Python executable hash mismatch. See Qt logs for details.");
	}
	else {
		// Report success
		qDebug() << "Python executable hash matches the expected hash.";
	}
}



TEST_F(PythonVirtualEnvTest, CreateVirtualEnv) {
	// Act
	bool created = virtualEnv->create();

	// Assert
	EXPECT_TRUE(created);
	QDir dir(tempDir);
	EXPECT_TRUE(dir.exists());
}

TEST_F(PythonVirtualEnvTest, ActivateVirtualEnv) {
	// Arrange
	ASSERT_TRUE(virtualEnv->create());

	// Act
	bool activated = virtualEnv->activate();

	// Assert
	EXPECT_TRUE(activated);
	// Additional checks can include environment variables if accessible
}

TEST_F(PythonVirtualEnvTest, GetPythonExecutable) {
	// Arrange
	ASSERT_TRUE(virtualEnv->create());

	// Act
	QString pythonExe = virtualEnv->getPythonExecutable();

	// Assert
	EXPECT_FALSE(pythonExe.isEmpty());
	// Optionally, check if the executable exists
	QFileInfo checkFile(pythonExe);
	EXPECT_TRUE(checkFile.exists() && checkFile.isExecutable());
}
