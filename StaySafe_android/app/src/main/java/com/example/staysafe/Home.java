package com.example.staysafe;

import static android.content.ContentValues.TAG;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton;
import com.google.firebase.auth.FirebaseAuth;
import com.google.firebase.auth.FirebaseUser;
import com.google.firebase.database.DataSnapshot;
import com.google.firebase.database.DatabaseError;
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;
import com.google.firebase.database.ValueEventListener;

import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttToken;

import java.util.ArrayList;
import java.util.List;

public class Home extends AppCompatActivity {
    private static final int CALL_PERMISSION_REQUEST = 1;
    private String userPhoneNumber = null;

    private static final String[] CRITICAL_KEYWORDS = {"DANGER", "CRITICAL", "FLOOD", "EVACUATE"};

    private TextView riskstatus, riskdesc, waterlevel, rainfall;
    private ExtendedFloatingActionButton fabsos;
    private LinearLayout risk_card;
    private MqttClientHelper mqttClientHelper;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_home);

        riskstatus = findViewById(R.id.riskstatus);
        riskdesc = findViewById(R.id.riskdesc);
        waterlevel = findViewById(R.id.waterlevel);
        rainfall = findViewById(R.id.rainfall);
        fabsos = findViewById(R.id.fabsos);
        risk_card = findViewById(R.id.risk_card);

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CALL_PHONE)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.CALL_PHONE},
                    CALL_PERMISSION_REQUEST);
        }
        MaterialToolbar toolbar = findViewById(R.id.toolbar);
        toolbar.setNavigationOnClickListener(v -> {
            startActivity(new Intent(Home.this, account.class));
        });

        fabsos.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                String phoneNumber = "27355144";
                Intent intent = new Intent(Intent.ACTION_DIAL);
                intent.setData(Uri.parse("tel:" + phoneNumber));
                startActivity(intent);
            }
        });
        fetchPhoneNumberFromFirebase();


        mqttClientHelper = new MqttClientHelper(this, "tcp://mqtt-dashboard.com:1883");
        mqttClientHelper.connect(new IMqttActionListener() {
            @Override
            public void onSuccess(IMqttToken asyncActionToken) {
                Log.d(TAG, "Connected to MQTT");
                subscribeToTopic("safty/state", riskstatus, "");
                subscribeToTopic("safty/risk", riskdesc, "");
                subscribeToTopic("safty/waterlevel", waterlevel, "");
                subscribeToTopic("safty/rainfall", rainfall, "");
            }

            @Override
            public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                Log.e(TAG, "Connection failed: " + exception.getMessage());
            }
        });
    }

    private void fetchPhoneNumberFromFirebase() {
        FirebaseUser currentUser = FirebaseAuth.getInstance().getCurrentUser();

        if (currentUser == null) {
            Log.e(TAG, "No logged in user found");
            return;
        }

        String uid = currentUser.getUid();

        DatabaseReference ref = FirebaseDatabase.getInstance()
                .getReference("Users")
                .child(uid)
                .child("phoneNumber");

        ref.addListenerForSingleValueEvent(new ValueEventListener() {
            @Override
            public void onDataChange(DataSnapshot snapshot) {
                if (snapshot.exists()) {
                    userPhoneNumber = snapshot.getValue(String.class);
                    Log.d(TAG, "Phone number retrieved: " + userPhoneNumber);
                } else {
                    Log.e(TAG, "Phone number not found in Firebase");
                }
            }

            @Override
            public void onCancelled(DatabaseError error) {
                Log.e(TAG, "Firebase error: " + error.getMessage());
            }
        });
    }

    private void subscribeToTopic(String topic, TextView textView, String unit) {
        mqttClientHelper.subscribe(topic, (t, message) -> {
            String payload = new String(message.getPayload()).trim();

            runOnUiThread(() -> {
                textView.setText(payload.toUpperCase() + unit);

                if (topic.equals("safty/state")) {
                    if (isCritical(payload)) {
                        Log.w(TAG, "Critical message on topic: " + topic + " → " + payload);
                        risk_card.setBackgroundColor(Color.parseColor("#800E13"));
                        riskdesc.setText(" Flood risk detected in your area!");
                        riskdesc.setTextColor(Color.parseColor("#FFD6D6"));
                        makeEmergencyCall();
                    } else {
                        risk_card.setBackgroundColor(Color.parseColor("#BFDBF7"));
                        riskdesc.setText("No flood risk in your area right now");
                        riskdesc.setTextColor(Color.parseColor("#E0F7FA"));
                    }
                } else {
                    textView.setText(payload + unit);
                }
            });
        });
    }

    private boolean isCritical(String message) {
        String upper = message.toUpperCase();
        for (String keyword : CRITICAL_KEYWORDS) {
            if (upper.contains(keyword)) return true;
        }
        return false;
    }

    private void makeEmergencyCall() {
        if (userPhoneNumber == null) {
            Log.e(TAG, "Phone number not loaded yet from Firebase");
            return;
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CALL_PHONE)
                == PackageManager.PERMISSION_GRANTED) {
            Intent callIntent = new Intent(Intent.ACTION_CALL);
            callIntent.setData(Uri.parse("tel:" + userPhoneNumber));
            startActivity(callIntent);
            Log.d(TAG, "Emergency call to: " + userPhoneNumber);
        } else {
            Log.e(TAG, "CALL_PHONE permission not granted");
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == CALL_PERMISSION_REQUEST) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Log.d(TAG, "CALL_PHONE permission granted");
            } else {
                Log.e(TAG, "CALL_PHONE permission denied");
            }
        }
    }

    public void onBackPressed() {
        super.onBackPressed();
         finishAffinity();

    }

}