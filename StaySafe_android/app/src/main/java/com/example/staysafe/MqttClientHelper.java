package com.example.staysafe;

import android.content.Context;
import android.util.Log;

import org.eclipse.paho.android.service.MqttAndroidClient;
import org.eclipse.paho.client.mqttv3.*;

public class MqttClientHelper {

    private static final String TAG = "MqttClientHelper";
    private MqttAndroidClient mqttAndroidClient;

    public MqttClientHelper(Context context, String brokerUrl) {
        mqttAndroidClient = new MqttAndroidClient(
                context,
                brokerUrl,
                MqttClient.generateClientId()
        );
    }

    public void connect(IMqttActionListener listener) {
        try {
            MqttConnectOptions options = new MqttConnectOptions();
            options.setCleanSession(true);
            options.setAutomaticReconnect(true);

            mqttAndroidClient.connect(options, null, listener);

        } catch (Exception e) {
            Log.e(TAG, "Connect error: " + e.getMessage());
        }
    }


    public void subscribe(String topic, IMqttMessageListener listener) {
        try {
            if (mqttAndroidClient.isConnected()) {
                mqttAndroidClient.subscribe(topic, 1, listener);
            } else {
                Log.e(TAG, "Subscribe failed: not connected");
            }
        } catch (Exception e) {
            Log.e(TAG, "Subscribe error: " + e.getMessage());
        }
    }


    public void subscribe(String[] topics, int[] qos, IMqttMessageListener[] callbacks) {
        try {
            if (mqttAndroidClient.isConnected()) {
                mqttAndroidClient.subscribe(topics, qos, callbacks);
            }
        } catch (Exception e) {
            Log.e(TAG, "Multi subscribe error: " + e.getMessage());
        }
    }

    public void publish(String topic, String payload, int qos) {
        try {
            if (mqttAndroidClient.isConnected()) {
                MqttMessage message = new MqttMessage(payload.getBytes());
                message.setQos(qos);
                mqttAndroidClient.publish(topic, message);
            }
        } catch (Exception e) {
            Log.e(TAG, "Publish error: " + e.getMessage());
        }
    }

    public boolean isConnected() {
        return mqttAndroidClient != null && mqttAndroidClient.isConnected();
    }
}
