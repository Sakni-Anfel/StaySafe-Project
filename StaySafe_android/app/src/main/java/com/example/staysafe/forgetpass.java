package com.example.staysafe;

import android.content.Intent;
import android.os.Bundle;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.textfield.TextInputEditText;
import com.google.firebase.auth.FirebaseAuth;

public class forgetpass extends AppCompatActivity {

    private TextView goBackIn;
    private MaterialButton btnReset;
    private TextInputEditText emailReset;
    private FirebaseAuth firebaseAuth;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_forgetpass);

        goBackIn   = findViewById(R.id.goBackIn);
        btnReset   = findViewById(R.id.btnReset);
        emailReset = findViewById(R.id.emailReset);

        firebaseAuth = FirebaseAuth.getInstance();

        goBackIn.setOnClickListener(v ->
                startActivity(new Intent(forgetpass.this, login.class)));

        btnReset.setOnClickListener(v -> {
            String email = emailReset.getText().toString().trim();
            if (email.isEmpty()) {
                emailReset.setError("Please enter your email");
                return;
            }
            firebaseAuth.sendPasswordResetEmail(email)
                    .addOnCompleteListener(task -> {
                        if (task.isSuccessful()) {
                            Toast.makeText(forgetpass.this,
                                    "Reset link sent to your email", Toast.LENGTH_SHORT).show();
                            startActivity(new Intent(forgetpass.this, login.class));
                            finish();
                        } else {
                            Toast.makeText(forgetpass.this,
                                    "Error: " + task.getException().getMessage(),
                                    Toast.LENGTH_SHORT).show();
                        }
                    });
        });
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
        startActivity(new Intent(forgetpass.this, login.class));
    }
}