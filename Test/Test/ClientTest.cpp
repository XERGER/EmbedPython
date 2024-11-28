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

		// Start the server using ServerController
		serverController->startServer();
		PythonClient client{};
		ASSERT_TRUE(client.waitForServerReady()) << "Server did not become ready in time.";
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
	QVariantList arguments;

	QSignalSpy readyReadSpy(&client, &PythonClient::dataReceived);

	client.connectToServer();
	const auto newExecutionId = QUuid::createUuid().toString();

	client.runScript(newExecutionId, script, arguments);

	ASSERT_TRUE(readyReadSpy.wait(5000)) << "Did not receive a response within the timeout.";

	// Check if the signal was emitted before accessing it
	ASSERT_GT(readyReadSpy.count(), 0) << "No signals captured by readyReadSpy.";

	// Safely retrieve the first signal
	QVariant firstSignal = readyReadSpy.takeFirst().at(0);

	ASSERT_TRUE(firstSignal.canConvert<QJsonObject>()) << "Received data is not a QJsonObject.";

	QJsonObject responseObj = firstSignal.toJsonObject();
	EXPECT_EQ(responseObj["status"].toString(), "success");
	EXPECT_EQ(responseObj["stdout"].toString().trimmed(), "30");

	qDebug() << "Test completed successfully.";
}

TEST_F(ClientTest, RunScriptWithArguments) {
	PythonClient client{};
	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";

	QString script = R"(
def calculate(a, b):
    return a + b

result = calculate(arg1, arg2)
print(result)
)";
	QVariantList arguments = { 10, 20 }; // Arguments to be passed to the script

	QSignalSpy readyReadSpy(&client, &PythonClient::dataReceived);
	const auto newExecutionId = QUuid::createUuid().toString();

	// Connect to the server and run the script
	client.connectToServer();
	client.runScript(newExecutionId, script, arguments);

	// Wait for the signal to be emitted
	ASSERT_TRUE(readyReadSpy.wait(5000)) << "Did not receive a response within the timeout.";

	// Check if the signal was emitted before accessing it
	ASSERT_GT(readyReadSpy.count(), 0) << "No signals captured by readyReadSpy.";

	// Safely retrieve the first signal
	QVariant firstSignal = readyReadSpy.takeFirst().at(0);
	ASSERT_TRUE(firstSignal.canConvert<QJsonObject>()) << "Received data is not a QJsonObject.";

	QJsonObject responseObj = firstSignal.toJsonObject();
	EXPECT_EQ(responseObj["status"].toString(), "success");
	EXPECT_EQ(responseObj["stdout"].toString().trimmed(), "30");
	EXPECT_EQ(responseObj["executionId"].toString().trimmed(), newExecutionId);

	qDebug() << "Test completed successfully with arguments.";
}
TEST_F(ClientTest, RunScriptWithNonDefaultPackage) {
	PythonClient client{};
	ASSERT_TRUE(client.waitForServerReady()) << "Server is not ready.";

	// Define a script with a non-default package import
	QString script = R"(
import numpy as np
result = np.add(10, 20)
print(result)
)";
	QVariantList arguments; // No arguments required
	QSignalSpy readyReadSpy(&client, &PythonClient::dataReceived);

	// Connect to the server and run the script
	const auto newExecutionId = QUuid::createUuid().toString();
	client.connectToServer();
	client.runScript(newExecutionId, script, arguments);

	// Wait for the signal to be emitted
	ASSERT_TRUE(readyReadSpy.wait(120000)) << "Did not receive a response within the timeout.";

	// Check if the signal was emitted before accessing it
	ASSERT_GT(readyReadSpy.count(), 0) << "No signals captured by readyReadSpy.";

	// Safely retrieve the first signal
	QVariant firstSignal = readyReadSpy.takeFirst().at(0);
	ASSERT_TRUE(firstSignal.canConvert<QJsonObject>()) << "Received data is not a QJsonObject.";
	QJsonObject responseObj = firstSignal.toJsonObject();

	// Validate the response
	EXPECT_EQ(responseObj["status"].toString(), "success");
	EXPECT_EQ(responseObj["stdout"].toString().trimmed(), "30"); // numpy adds integers and returns 30
	EXPECT_EQ(responseObj["executionId"].toString().trimmed(), newExecutionId);

	qDebug() << "Test completed successfully with non-default package." << responseObj;
}
