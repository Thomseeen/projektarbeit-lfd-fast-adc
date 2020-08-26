// sync_consume.cpp
//
// This is a Paho MQTT C++ client, sample application.
//
// This application is an MQTT consumer/subscriber using the C++ synchronous
// client interface, which uses the queuing API to receive messages.
//
// The sample demonstrates:
//  - Connecting to an MQTT server/broker
//  - Subscribing to multiple topics
//  - Receiving messages through the queueing consumer API
//  - Recieving and acting upon commands via MQTT topics
//  - Manual reconnects
//  - Using a persistent (non-clean) session
//

/*******************************************************************************
 * Copyright (c) 2013-2017 Frank Pagliughi <fpagliughi@mindspring.com>
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Frank Pagliughi - initial implementation and documentation
 *******************************************************************************/

#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <thread>
#include <chrono>

#include "mqtt/client.h"
#include "adc_reading.h"

using namespace std;
using namespace std::chrono;

const string SERVER_ADDRESS{ "tcp://192.168.178.16:1883" };
const string CLIENT_ID{ "mfd-c" };
const string TOPICS{ "lfd1/#" };
const int QOS{ 0 };

// --------------------------------------------------------------------------
// Simple function to manually reconect a client.

bool try_reconnect(mqtt::client& cli)
{
	constexpr int N_ATTEMPT = 30;

	for (int i = 0; i < N_ATTEMPT && !cli.is_connected(); ++i) {
		try {
			cli.reconnect();
			return true;
		}
		catch (const mqtt::exception&) {
			this_thread::sleep_for(seconds(1));
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	mqtt::connect_options connOpts;
	connOpts.set_keep_alive_interval(20);
	connOpts.set_clean_session(false);

	mqtt::client cli(SERVER_ADDRESS, CLIENT_ID);

	try {
		cout << "Connecting to the MQTT server..." << flush;
		mqtt::connect_response rsp = cli.connect(connOpts);
		cout << "OK\n" << endl;

		if (!rsp.is_session_present()) {
			std::cout << "Subscribing to topics..." << std::flush;
			cli.subscribe(TOPICS, QOS);
			std::cout << "OK" << std::endl;
		}
		else {
			cout << "Session already present. Skipping subscribe." << std::endl;
		}

		// Consume messages

		while (true) {
			auto msg = cli.consume_message();

			if (!msg) {
				if (!cli.is_connected()) {
					cout << "Lost connection. Attempting reconnect" << endl;
					if (try_reconnect(cli)) {
						cli.subscribe(TOPICS, QOS);
						cout << "Reconnected" << endl;
						continue;
					}
					else {
						cout << "Reconnect failed." << endl;
					}
				}
				else {
					cout << "An error occurred retrieving messages." << endl;
				}
				break;
			}

			// Struct to hold measurement
			AdcReading* adc_reading;

			// Char/byte buffer to copy recieved message in
			char buffer[sizeof(AdcReading)];

			// Get payload of recieved message
			mqtt::binary payload = msg->get_payload();

			// Copy recieved message (mqtt::binary is just a std::string) into char/byte buffer
			payload.copy(buffer, sizeof(AdcReading), 0);

			// Cast from char/byte buffer into measurement struct
			adc_reading = (AdcReading*) buffer;

			cout << dec << "Pin " << (unsigned short) adc_reading->pin_no << " reads " << (float)adc_reading->value / (float)0xFFF0 * 1.8f << " at " << (unsigned long long) adc_reading->seq_no << endl;
		}

		// Disconnect

		cout << "\nDisconnecting from the MQTT server..." << flush;
		cli.disconnect();
		cout << "OK" << endl;
	}
	catch (const mqtt::exception& exc) {
		cerr << exc.what() << endl;
		return 1;
	}

	return 0;
}