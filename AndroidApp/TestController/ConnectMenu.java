package com.zackmalig.blindscontroller;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import java.io.DataOutputStream;
import java.io.IOException;
import java.net.Socket;

public class ConnectMenu extends AppCompatActivity implements View.OnClickListener {

    public final static String PREF_IP = "PREF_IP_ADDRESS";
    public final static String PREF_PORT = "PREF_PORT_NUMBER";

    TextView messageText, testResult;
    Button sendData;
    EditText ipAddress, portNumber, parameterValue;


    // shared preferences to be saved when user closes the application
    SharedPreferences.Editor editor;
    SharedPreferences sharedPreferences;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_connect_menu);

        // get shared preferences
        sharedPreferences = getSharedPreferences("HTTP_HELPER_PREFS", Context.MODE_PRIVATE);
        editor = sharedPreferences.edit();

        // test code for intent
        Intent intent = getIntent();
        String message = intent.getStringExtra(MainActivity.EXTRA_MESSAGE);
        //display data passed through intent
        messageText = (TextView) findViewById(R.id.newMessage);
        messageText.setText(message);

        // assign textView
        testResult = (TextView) findViewById(R.id.textViewTestResult);

        // assign button
        sendData = (Button) findViewById(R.id.button_sendData);
        sendData.setOnClickListener(this);

        // assign edit text
        ipAddress = (EditText) findViewById(R.id.editTextIPAddress);
        portNumber = (EditText) findViewById(R.id.editTextPortNumber);
        parameterValue = (EditText) findViewById(R.id.editTextData);

        // get IP address and port number from last user run
        ipAddress.setText(sharedPreferences.getString(PREF_IP, "IP Address"));
        portNumber.setText(sharedPreferences.getString(PREF_PORT,"Port #"));
    }


    @Override
    public void onClick(View view) {
        // get ip address
        String ip = ipAddress.getText().toString().trim();
        // get port number
        String portString = portNumber.getText().toString().trim();


        // save IP address and port for future use
        editor.putString(PREF_IP, ip);
        editor.putString(PREF_PORT, portString);
        editor.commit(); // save new values

        switch(view.getId()) {
            case R.id.button_sendData:
                // get data value
                String dataValue = parameterValue.getText().toString().trim();

                // output to socket
                try {
                    int portValue = Integer.parseInt(portString);
                    Socket socket = new Socket(ip, portValue);
                    DataOutputStream DOS = new DataOutputStream(socket.getOutputStream());
                    DOS.writeUTF(dataValue);

                    // declare success
                    testResult.setText("Success!");
                } catch (IOException e) {
                    e.printStackTrace();
                    testResult.setText("Failure");
                } catch (NumberFormatException e) {
                    portNumber.setText("Invalid Port");
                    testResult.setText("Failure");
                }
                break;
        }

    }

}
