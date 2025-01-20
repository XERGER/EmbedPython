#include "../pch.h"
#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QTimer>
#include "Client/PythonClient.h"
#include "Client/ServerController.h"


class ClientTest : public ::testing::Test {
protected:
	ServerController* serverController = nullptr;

	void SetUp() override {
		const auto enginePath = QCoreApplication::applicationDirPath() + "/PythonEngine.exe";

		serverController = new ServerController(enginePath);
		ASSERT_NE(serverController, nullptr);

		serverController->startServer();
	}

	void TearDown() override {
		if (serverController) {
			serverController->stopServer();
			delete serverController;
		}
	}
};

TEST_F(ClientTest, ServerStartsSuccessfully) {
	ASSERT_TRUE(serverController->isServerRunning()) << "Server is not running after starting.";
}

TEST_F(ClientTest, ServerStopsSuccessfully) {
	serverController->stopServer();
	ASSERT_FALSE(serverController->isServerRunning()) << "Server is still running after stop.";
}

TEST_F(ClientTest, RunScriptSuccess) {
	PythonClient client{};
	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";

	const auto script = "result = 10 + 20\nprint(result)";

	QSignalSpy readyReadSpy(&client, &PythonClient::scriptExecutionFinished);

	const auto newExecutionId = QUuid::createUuid().toString();

	client.runScript(newExecutionId, script, {});

	ASSERT_TRUE(readyReadSpy.wait(120000)) << "Did not receive a response within the timeout.";

	// Check if the signal was emitted before accessing it
	ASSERT_GT(readyReadSpy.count(), 0) << "No signals captured by readyReadSpy.";

	// Safely retrieve the first signal
	QList<QVariant> arguments = readyReadSpy.takeFirst();
	const auto uninstallation = arguments.at(0).value<PythonResult>();

	EXPECT_TRUE(uninstallation.isSuccess());
	EXPECT_EQ(uninstallation.getOutput(), "30\r\n");
	EXPECT_EQ(uninstallation.getExecutionId(), newExecutionId);


	qDebug() << "Test completed successfully." << uninstallation.toJson();
}

// TEST_F(ClientTest, RunScriptWithArguments) {
// 	PythonClient client{};
// 	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";
// 
// 	QString script = R"(
// def calculate(a, b):
//     return a + b
// 
// result = calculate(arg[0], arg[1])
// print(result)
// )";
// 	QVariantList arguments = { 10, 20 }; // Arguments to be passed to the script
// 
// 	QSignalSpy readyReadSpy(&client, &PythonClient::scriptExecutionFinished);
// 	const auto newExecutionId = QUuid::createUuid().toString();
// 
// 	// Connect to the server and run the script
// 	client.connectToServer();
// 	client.runScript(newExecutionId, script, arguments);
// 
// 	// Wait for the signal to be emitted
// 	ASSERT_TRUE(readyReadSpy.wait(5000)) << "Did not receive a response within the timeout.";
// 
// 	// Check if the signal was emitted before accessing it
// 	ASSERT_GT(readyReadSpy.count(), 0) << "No signals captured by readyReadSpy.";
// 
// 	// Safely retrieve the first signal
// 	QList<QVariant> results = readyReadSpy.takeFirst();
// 	const auto uninstallation = results.at(0).value<PythonResult>();
// 
// 	qDebug() << "Test completed successfully with arguments." << uninstallation.toJson();
// }

// TEST_F(ClientTest, InstallRunUninstallNumpy) {
// 	PythonClient client{};
// 	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";
// 
// 	// Step 1: Install numpy
// 	{
// 		QSignalSpy installSpy(&client, &PythonClient::packageOperationFinished);
// 		client.connectToServer();
// 
// 		QString packageName = "numpy";
// 		QString installExecutionId = QUuid::createUuid().toString();
// 		client.installPackage(installExecutionId, packageName);
// 
// 		ASSERT_TRUE(installSpy.wait(60000)) << "No response received for installPackage.";
// 		ASSERT_GT(installSpy.count(), 0) << "No signals captured for installPackage.";
// 
// 		auto installResult = installSpy.takeFirst().at(0).value<PythonResult>();
// 		ASSERT_TRUE(installResult.isSuccess()) << "Failed to install package 'numpy'.";
// 		EXPECT_EQ(installResult.getExecutionId(), installExecutionId);
// 		qDebug() << "Install package test completed successfully.";
// 	}
// 
// 	// Step 2: Run script using numpy
// 	{
// 		QString script = R"(
// import numpy as np
// result = np.add(10, 20)
// print(result)
// )";
// 
// 		QVariantList arguments; // No arguments required
// 		QSignalSpy scriptSpy(&client, &PythonClient::scriptExecutionFinished);
// 
// 		QString scriptExecutionId = QUuid::createUuid().toString();
// 		client.runScript(scriptExecutionId, script, arguments);
// 
// 		ASSERT_TRUE(scriptSpy.wait(30000)) << "Did not receive a response for script execution within the timeout.";
// 		ASSERT_GT(scriptSpy.count(), 0) << "No signals captured for scriptExecutionFinished.";
// 
// 		QList<QVariant> scriptResults = scriptSpy.takeFirst();
// 		PythonResult scriptResult = scriptResults.at(0).value<PythonResult>();
// 
// 		EXPECT_EQ(scriptResult.getExecutionId(), scriptExecutionId);
// 		EXPECT_TRUE(scriptResult.isSuccess()) << "Script execution failed.";
// 		qDebug() << "Script executed successfully:" << scriptResult.toJson();
// 	}
// 
// 	// Step 3: Uninstall numpy
// 	{
// 		QSignalSpy uninstallSpy(&client, &PythonClient::packageOperationFinished);
// 		QString packageName = "numpy";
// 		QString uninstallExecutionId = QUuid::createUuid().toString();
// 		client.uninstallPackage(uninstallExecutionId, packageName);
// 
// 		ASSERT_TRUE(uninstallSpy.wait(60000)) << "No response received for uninstallPackage.";
// 		ASSERT_GT(uninstallSpy.count(), 0) << "No signals captured for uninstallPackage.";
// 
// 		auto uninstallResult = uninstallSpy.takeFirst().at(0).value<PythonResult>();
// 		ASSERT_TRUE(uninstallResult.isSuccess()) << "Failed to uninstall package 'numpy'.";
// 		EXPECT_EQ(uninstallResult.getExecutionId(), uninstallExecutionId);
// 		qDebug() << "Uninstall package test completed successfully.";
// 	}
// 
// 	qDebug() << "Install, run, and uninstall numpy test completed successfully.";
// }

TEST_F(ClientTest, InstallRunUninstallRequests) {
	PythonClient client{};

	// Arrange
	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";

	// Step 1: Install requests
	{
		// Arrange
		QSignalSpy installSpy(&client, &PythonClient::packageOperationFinished);
		QString packageName = "requests";
		QString installExecutionId = QUuid::createUuid().toString();

		// Act
		client.connectToServer();
		client.installPackage(installExecutionId, packageName);

		// Assert
		ASSERT_TRUE(installSpy.wait(60000)) << "No response received for installPackage.";
		ASSERT_GT(installSpy.count(), 0) << "No signals captured for installPackage.";

		auto installResult = installSpy.takeFirst().at(0).value<PythonResult>();
		ASSERT_TRUE(installResult.isSuccess()) << "Failed to install package 'requests'.";
		EXPECT_EQ(installResult.getExecutionId(), installExecutionId);
		EXPECT_EQ(installResult.getErrorOutput(), "");
		EXPECT_EQ(installResult.getErrorCode(), 0);
		//EXPECT_GT(installResult.getExecutionTime(), 0);

		qDebug() << "Install package 'requests' completed successfully.";
	}

	// Step 2: Check SSL Availability
	{
		// Arrange
		QString sslCheckScript = R"(
import ssl
def check_ssl():
    try:
        ssl.create_default_context()
        print("SSL is available.")
    except Exception as e:
        print(f"SSL is not available: {e}")
check_ssl())";
		QVariantList sslArguments;
		QSignalSpy sslSpy(&client, &PythonClient::scriptExecutionFinished);
		QString sslExecutionId = QUuid::createUuid().toString();

		// Act
		client.runScript(sslExecutionId, sslCheckScript, sslArguments);

		// Assert
		ASSERT_TRUE(sslSpy.wait(30000)) << "Did not receive a response for SSL check within the timeout.";
		ASSERT_GT(sslSpy.count(), 0) << "No signals captured for SSL check.";

		QList<QVariant> sslResults = sslSpy.takeFirst();
		PythonResult sslResult = sslResults.at(0).value<PythonResult>();
		EXPECT_EQ(sslResult.getExecutionId(), sslExecutionId);
		EXPECT_TRUE(sslResult.isSuccess()) << "SSL check script execution failed.";
		EXPECT_EQ(sslResult.getOutput().trimmed(), "SSL is available.");
		EXPECT_EQ(sslResult.getErrorOutput(), "");
		EXPECT_EQ(sslResult.getErrorCode(), 0);
		EXPECT_GT(sslResult.getExecutionTime(), 0);

		if (!sslResult.isSuccess()) {
			GTEST_SKIP() << "SSL module is not available. Skipping the requests test.";
		}

		QString sslOutput = sslResult.getOutput().trimmed();
		if (sslOutput != "SSL is available.") {
			GTEST_SKIP() << "SSL module is not available. Skipping the requests test.";
		}

		qDebug() << "SSL is available. Proceeding with requests test.";
	}

	// Step 3: Run script using requests
	{
		// Arrange
		QString script = R"(
import requests
def fetch_status(url):
    try:
        response = requests.get(url)
        return response.status_code
    except Exception as e:
        return str(e)
result = fetch_status("https://httpbin.org/get")
print(result))";
		QVariantList arguments;
		QSignalSpy scriptSpy(&client, &PythonClient::scriptExecutionFinished);
		QString scriptExecutionId = QUuid::createUuid().toString();

		// Act
		client.runScript(scriptExecutionId, script, arguments);

		// Assert
		ASSERT_TRUE(scriptSpy.wait(30000)) << "Did not receive a response for script execution within the timeout.";
		ASSERT_GT(scriptSpy.count(), 0) << "No signals captured for scriptExecutionFinished.";

		QList<QVariant> scriptResults = scriptSpy.takeFirst();
		PythonResult scriptResult = scriptResults.at(0).value<PythonResult>();
		EXPECT_EQ(scriptResult.getExecutionId(), scriptExecutionId);
		EXPECT_TRUE(scriptResult.isSuccess()) << "Script execution failed.";
		EXPECT_EQ(scriptResult.getOutput().trimmed(), "200");
		EXPECT_EQ(scriptResult.getErrorOutput(), "");
		EXPECT_EQ(scriptResult.getErrorCode(), 0);
		EXPECT_GT(scriptResult.getExecutionTime(), 0);

		qDebug() << "Script executed successfully:" << scriptResult.toJson();
	}

	// Step 4: Uninstall requests
	{
		// Arrange
		QSignalSpy uninstallSpy(&client, &PythonClient::packageOperationFinished);
		QString packageName = "requests";
		QString uninstallExecutionId = QUuid::createUuid().toString();

		// Act
		client.uninstallPackage(uninstallExecutionId, packageName);

		// Assert
		ASSERT_TRUE(uninstallSpy.wait(60000)) << "No response received for uninstallPackage.";
		ASSERT_GT(uninstallSpy.count(), 0) << "No signals captured for uninstallPackage.";

		auto uninstallResult = uninstallSpy.takeFirst().at(0).value<PythonResult>();
		ASSERT_TRUE(uninstallResult.isSuccess()) << "Failed to uninstall package 'requests'.";
		EXPECT_EQ(uninstallResult.getExecutionId(), uninstallExecutionId);
		EXPECT_TRUE(uninstallResult.getOutput().contains("Uninstalled package: requests"));
		EXPECT_EQ(uninstallResult.getErrorOutput(), "");
		EXPECT_EQ(uninstallResult.getErrorCode(), 0);
		EXPECT_GT(uninstallResult.getExecutionTime(), 0);

		qDebug() << "Uninstall package 'requests' completed successfully.";
	}

	qDebug() << "Install, run, and uninstall 'requests' package test completed successfully.";
}


// TEST_F(ClientTest, RunScriptWithNonDefaultPackage) {
// 	PythonClient client{};
// 	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";
// 
// 	// Define a script with a non-default package import
// 	QString script = R"(
// import numpy as np
// result = np.add(10, 20)
// print(result)
// )";
// 	QVariantList arguments; // No arguments required
// 	QSignalSpy readyReadSpy(&client, &PythonClient::scriptExecutionFinished);
// 
// 	// Connect to the server and run the script
// 	const auto newExecutionId = QUuid::createUuid().toString();
// 	client.connectToServer();
// 	client.runScript(newExecutionId, script, arguments);
// 
// 	// Wait for the signal to be emitted
// 	ASSERT_TRUE(readyReadSpy.wait(30000)) << "Did not receive a response within the timeout.";
// 
// 	// Check if the signal was emitted before accessing it
// 	ASSERT_GT(readyReadSpy.count(), 0) << "No signals captured by readyReadSpy.";
// 
// 	// Safely retrieve the first signal
// 	QList<QVariant> results = readyReadSpy.takeFirst();
// 	const auto uninstallation = results.at(0).value<PythonResult>();
// 	EXPECT_EQ(uninstallation.getExecutionId(), newExecutionId);
// 
// 	qDebug() << "Test completed successfully with non-default package." << uninstallation.toJson();
// }
// 
// 
// TEST_F(ClientTest, InstallPackage) {
// 	PythonClient client{};
// 	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";
// 
// 	QSignalSpy spy(&client, &PythonClient::packageOperationFinished);
// 	client.connectToServer();
// 
// 	QString packageName = "numpy";
// 	const auto newExecutionId = QUuid::createUuid().toString();
// 	client.installPackage(newExecutionId, packageName);
// 
// 	ASSERT_TRUE(spy.wait(60000)) << "No response received for installPackage.";
// 	ASSERT_GT(spy.count(), 0) << "No signals captured for installPackage.";
// 
// 	auto result = spy.takeFirst().at(0).value<PythonResult>();
// 	ASSERT_TRUE(result.isSuccess()) << "Failed to install package.";
// 	EXPECT_EQ(result.getExecutionId(), newExecutionId);
// 
// 	qDebug() << "Install package test completed successfully.";
// }
// 
// TEST_F(ClientTest, UninstallPackage) {
// 	PythonClient client{};
// 	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";
// 
// 	QSignalSpy spy(&client, &PythonClient::packageOperationFinished);
// 	client.connectToServer();
// 
// 	QString packageName = "numpy";
// 	const auto newExecutionId = QUuid::createUuid().toString();
// 	client.uninstallPackage(newExecutionId, packageName);
// 
// 	ASSERT_TRUE(spy.wait(60000)) << "No response received for uninstallPackage.";
// 	ASSERT_GT(spy.count(), 0) << "No signals captured for uninstallPackage.";
// 
// 	auto result = spy.takeFirst().at(0).value<PythonResult>();
// 	ASSERT_TRUE(result.isSuccess()) << "Failed to uninstall package.";
// 	EXPECT_EQ(result.getExecutionId(), newExecutionId);
// 	qDebug() << "Uninstall package test completed successfully.";
//}

// 
// TEST_F(ClientTest, ReinstallPackage) {
// 	PythonClient client{};
// 	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";
// 
// 	QSignalSpy spy(&client, &PythonClient::packageOperationFinished);
// 	client.connectToServer();
// 
// 	QString packageName = "numpy";
// 	client.reinstallPackage(packageName);
// 
// 	ASSERT_TRUE(spy.wait(60000)) << "No response received for reinstallPackage.";
// 	ASSERT_GT(spy.count(), 0) << "No signals captured for reinstallPackage.";
// 
// 	auto result = spy.takeFirst().at(0).value<PythonResult>();
// 	ASSERT_TRUE(result.isSuccess()) << "Failed to reinstall package.";
// 	qDebug() << "Reinstall package test completed successfully.";
// }
// 
// TEST_F(ClientTest, GetPackageInfo) {
// 	PythonClient client{};
// 	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";
// 
// 	QString packageName = "numpy";
// 	auto packageInfo = client.getPackageInfo(packageName);
// 
// 	ASSERT_FALSE(packageInfo.isEmpty()) << "Failed to retrieve package information.";
// 	qDebug() << "Package info:" << packageInfo;
// }

// TEST_F(ClientTest, SearchPackage) {
// 	PythonClient client{};
// 	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";
// 
// 	QString query = "numpy";
// 	auto searchResults = client.searchPackage(query);
// 
// 	ASSERT_FALSE(searchResults.isEmpty()) << "No results found for searchPackage.";
// 	qDebug() << "Search results:" << searchResults;
// }
// 
// TEST_F(ClientTest, ListInstalledPackages) {
// 	PythonClient client{};
// 	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";
// 
// 	auto installedPackages = client.listInstalledPackages();
// 	QSignalSpy spy(&client, &PythonClient::packageOperationFinished);
// 
// 	ASSERT_TRUE(spy.wait(10000)) << "No response received for uninstallPackage.";
// 	auto result = spy.takeFirst().at(0).value<PythonResult>();
// 	ASSERT_TRUE(result.isSuccess()) << "Failed to uninstall package.";
// 	ASSERT_FALSE(installedPackages.isEmpty()) << "No installed packages retrieved.";
// 	qDebug() << "Installed packages:" << installedPackages;
// }
// 
// TEST_F(ClientTest, UpgradeAllPackages) {
// 	PythonClient client{};
// 	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";
// 
// 	QSignalSpy spy(&client, &PythonClient::packageOperationProgress);
// 	client.connectToServer();
// 
// 	client.upgradeAllPackages();
// 
// 	ASSERT_TRUE(spy.wait(120000)) << "No response received for upgradeAllPackages.";
// 	ASSERT_GT(spy.count(), 0) << "No signals captured for upgradeAllPackages.";
// 
// 	qDebug() << "Upgrade all packages test completed successfully.";
// }
