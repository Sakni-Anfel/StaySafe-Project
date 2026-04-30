package com.example.staysafe;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
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
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class signup extends AppCompatActivity {
    private TextView goToSignIn;
    private EditText fullNameet,emailet,phoneNumberet,yourPasswordet,confirmPasswordet,adresset;
    private View signUp;
    private FirebaseAuth firebaseAuth;
    private String fullNameS,emailS,phoneNumberS,yourPasswordS,confirmPasswordS;
    private static final String EMAIL_REGEX=
            "^[_A-Za-z0-9-\\+]+(\\.[_A-Za-z0-9]+)*@"
                    +"[A-Za-z0-9-]+(\\.[A-Za-z0-9]+)*(\\.[A-Za-z]{2,})$";
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_signup);

        fullNameet=findViewById(R.id.fullName);
        emailet=findViewById(R.id.email);
        phoneNumberet=findViewById(R.id.phoneNumber);
        yourPasswordet=findViewById(R.id.yourPassword);
        confirmPasswordet=findViewById(R.id.confirmPassword);
        signUp=findViewById(R.id.signUp);
        goToSignIn = findViewById(R.id.goToSignIn);
        goToSignIn.setOnClickListener(v ->{
            startActivity(new Intent(signup.this,login.class));
        });
        firebaseAuth= FirebaseAuth.getInstance();
        signUp.setOnClickListener(v ->{
            if (validate()) {
                firebaseAuth.createUserWithEmailAndPassword(emailS,yourPasswordS).addOnCompleteListener(new OnCompleteListener<AuthResult>() {
                    @Override
                    public void onComplete(@NonNull Task<AuthResult> task) {
                        if (task.isSuccessful()) {
                            sendEmailVerification();
                        }else{
                            Toast.makeText(signup.this, "register failed", Toast.LENGTH_SHORT).show();
                        }
                    }
                });

            }
        });
    }

    private void sendEmailVerification() {
        FirebaseUser user = firebaseAuth.getCurrentUser();
        if (user != null ){
            user.sendEmailVerification().addOnCompleteListener(new OnCompleteListener<Void>() {
                @Override
                public void onComplete(@NonNull Task<Void> task) {
                    if (task.isSuccessful()){

                        sendUserData();
                        Toast.makeText(signup.this, "registration done pls check your email", Toast.LENGTH_SHORT).show();
                        firebaseAuth.signOut();
                        startActivity(new Intent(signup.this,login.class));
                        finish();
                    }else {
                        Toast.makeText(signup.this, "registration failed", Toast.LENGTH_SHORT).show();
                    }
                }
            });
        }
    }
    private void sendUserData() {
        phoneNumberS=phoneNumberet.getText().toString();


        FirebaseDatabase firebaseDatabase = FirebaseDatabase.getInstance();

        DatabaseReference myRef = firebaseDatabase.getReference("Users");
        if ((phoneNumberS.length()==8 || phoneNumberS.isEmpty()))
        { User user = new User(fullNameS, phoneNumberS,emailS);

            myRef.child(""+firebaseAuth.getUid()).setValue(user);}
        else {
            phoneNumberet.setError("phone number not valide");
            adresset.setError("country not valide");
        }
    }




    private boolean validate() {
        boolean result=false;
        fullNameS=fullNameet.getText().toString();
        emailS=emailet.getText().toString();
        yourPasswordS=yourPasswordet.getText().toString();
        confirmPasswordS=confirmPasswordet.getText().toString();
        phoneNumberS=phoneNumberet.getText().toString();

        if (fullNameS.isEmpty() & emailS.isEmpty()  & yourPasswordS.isEmpty() & confirmPasswordS.isEmpty()) {
            fullNameet.setError("full name is required");
            emailet.setError("email is required");
            yourPasswordet.setError("password is required");
            confirmPasswordet.setError("password is required");}
        else if (fullNameS.isEmpty()||fullNameS.length()<7){
            fullNameet.setError("full name not valide");
        }
        else if (!isValidEmail(emailS)) {
            emailet.setError("email not valide");
        }  else if (yourPasswordS.length()<7){
            yourPasswordet.setError("password not valide");
        } else if (!yourPasswordS.equals(confirmPasswordS)) {
            confirmPasswordet.setError("password not match");
        }   else {result=true;}
        return result;
    }

    public static boolean isValidEmail(String email){
        Pattern pattern = Pattern.compile(EMAIL_REGEX);
        Matcher matcher= pattern.matcher(email);
        return matcher.matches();
    }
    @Override
    public void onBackPressed() {
        super.onBackPressed();
        startActivity(new Intent(signup.this,main.class));


    }
}