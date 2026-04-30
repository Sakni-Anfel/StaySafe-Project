package com.example.staysafe;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import com.google.firebase.auth.FirebaseAuth;
import com.google.firebase.auth.FirebaseUser;
import com.google.firebase.database.DataSnapshot;
import com.google.firebase.database.DatabaseError;
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;
import com.google.firebase.database.ValueEventListener;

public class account extends AppCompatActivity {

    private Button editProfile, logOut;
    private EditText emailProfile, fullNameProfile, phoneNumberProfile;
    private FirebaseAuth firebaseAuth;
    private DatabaseReference reference;
    private boolean isEditing = false; // tracks current mode

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_account);

        editProfile        = findViewById(R.id.editProfile);
        logOut             = findViewById(R.id.logOut);
        emailProfile       = findViewById(R.id.emailProfile);
        fullNameProfile    = findViewById(R.id.fullNameProfil);
        phoneNumberProfile = findViewById(R.id.phoneNumber);

        firebaseAuth = FirebaseAuth.getInstance();
        FirebaseUser loggedUser = firebaseAuth.getCurrentUser();

        if (loggedUser == null) {
            startActivity(new Intent(account.this, main.class));
            finishAffinity();
            return;
        }

        reference = FirebaseDatabase.getInstance()
                .getReference("Users")
                .child(loggedUser.getUid());

        // Load user data
        reference.addValueEventListener(new ValueEventListener() {
            @Override
            public void onDataChange(@NonNull DataSnapshot snapshot) {
                if (!snapshot.exists()) return;
                String fullName = snapshot.child("fullName").getValue()   != null
                        ? snapshot.child("fullName").getValue().toString()   : "";
                String email    = snapshot.child("email").getValue()       != null
                        ? snapshot.child("email").getValue().toString()       : "";
                String phone    = snapshot.child("phoneNumber").getValue() != null
                        ? snapshot.child("phoneNumber").getValue().toString() : "";
                fullNameProfile.setText(fullName);
                emailProfile.setText(email);
                phoneNumberProfile.setText(phone);
            }
            @Override
            public void onCancelled(@NonNull DatabaseError error) {
                Toast.makeText(account.this, "Error: " + error.getMessage(), Toast.LENGTH_SHORT).show();
            }
        });

        // Logout
        logOut.setOnClickListener(v -> {
            firebaseAuth.signOut();
            startActivity(new Intent(account.this, main.class));
            finishAffinity();
        });

        // Single click listener that toggles between Edit and Save
        editProfile.setOnClickListener(v -> {
            if (!isEditing) {
                // Switch to EDIT mode
                isEditing = true;
                fullNameProfile.setFocusableInTouchMode(true);
                phoneNumberProfile.setFocusableInTouchMode(true);
                fullNameProfile.requestFocus();
                editProfile.setText("Save");

            } else {
                // Switch to SAVE mode
                String editFullName = fullNameProfile.getText().toString().trim();
                String editPhone    = phoneNumberProfile.getText().toString().trim();

                boolean nameValid  = editFullName.length() >= 7;
                boolean phoneValid = editPhone.isEmpty() || editPhone.length() == 8;

                if (!nameValid) {
                    fullNameProfile.setError("Name must be at least 7 characters");
                    return;
                }
                if (!phoneValid) {
                    phoneNumberProfile.setError("Phone number must be 8 digits");
                    return;
                }

                reference.child("fullName").setValue(editFullName);
                reference.child("phoneNumber").setValue(editPhone);
                Toast.makeText(account.this, "Profile updated", Toast.LENGTH_SHORT).show();

                // Switch back to view mode
                isEditing = false;
                fullNameProfile.setFocusable(false);
                phoneNumberProfile.setFocusable(false);
                fullNameProfile.clearFocus();
                phoneNumberProfile.clearFocus();
                editProfile.setText("Edit Profile");
            }
        });
    }
}