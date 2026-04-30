package com.example.staysafe;

import android.app.ProgressDialog;
import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.firebase.auth.AuthResult;
import com.google.firebase.auth.FirebaseAuth;
import com.google.firebase.auth.FirebaseUser;

public class login extends AppCompatActivity {
    private TextView goToSignUp;
    private TextView forgetPassword;
    private EditText mail,password;
    private Button signIn;

    private FirebaseAuth firebaseAuth;
    private ProgressDialog progressDialog;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_login);
        goToSignUp=findViewById(R.id.goToSignUp);
        forgetPassword=findViewById(R.id.forgetPassword);
        mail=findViewById(R.id.mail);
        password=findViewById(R.id.password);
        signIn=findViewById(R.id.signIn);



        progressDialog = new ProgressDialog(this);

        firebaseAuth = FirebaseAuth.getInstance();
        goToSignUp.setOnClickListener(v -> {
            startActivity(new Intent(login.this,signup.class));
        });
        forgetPassword.setOnClickListener(v ->{
            startActivity(new Intent(login.this,forgetpass.class));
        });

        signIn.setOnClickListener(v -> {
            if (validateInput()) {
                progressDialog.setMessage("Please wait...");
                progressDialog.show();
                firebaseAuth.signInWithEmailAndPassword(mail.getText().toString(), password.getText().toString()).addOnCompleteListener(new OnCompleteListener<AuthResult>() {

                    @Override
                    public void onComplete(@NonNull Task<AuthResult> task) {
                        if (task.isSuccessful()) {
                            verifyEmail();
                            progressDialog.dismiss();

                        } else {
                            Toast.makeText(login.this, "Please verify that your data is correct", Toast.LENGTH_SHORT).show();
                            progressDialog.dismiss();
                        }
                    }
                });
            }


        });

    }
    private boolean validateInput() {
        boolean result = true;
        String email = mail.getText().toString().trim();
        String epassword = password.getText().toString().trim();
        if (email.isEmpty() & epassword.isEmpty()) {
            mail.setError("email is required");
            password.setError("password is required");
            result=false;
        } else if (email.isEmpty()) {mail.setError("email is required");
            result=false;
        } else if (epassword.isEmpty()) {password.setError("password is required");
            result=false;
        }
        return result;
    }
    private void verifyEmail() {
        FirebaseUser connectedUser = firebaseAuth.getCurrentUser();
        boolean isEmailFlag = connectedUser.isEmailVerified();
        if (isEmailFlag){
            startActivity(new Intent(login.this,Home.class));
        } else{
            Toast.makeText(this, "Please Verify your email's account", Toast.LENGTH_SHORT).show();
            firebaseAuth.signOut();
        }
    }
    @Override
    public void onBackPressed() {
        super.onBackPressed();
        startActivity(new Intent(login.this,main.class));


    }
}