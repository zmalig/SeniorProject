package com.miniblinds.controller;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import com.google.gson.Gson;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.InetSocketAddress;
import java.net.Socket;

public class Connect extends AppCompatActivity implements View.OnClickListener{

    //constants
    private EditText ssidText, passwordText, blindNickname;
    private Button connect_button;
    private TextView debugText;

    private String tempNickname;

    private SharedPreferences sharedPref;
    private SharedPreferences.Editor editor;

    private Socket socket;

    // constants: Initial IP, Port, SSID and Password of ESP
    private final String ipAddress = "192.168.1.103";
    private final int portNumber = 4989;
    private final String ESP_SSID = "ssid";
    private final String ESP_PASS = "password";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_connect);

        //get shared preferences
        sharedPref = getSharedPreferences("com.controller.BLINDS_PREFS", Context.MODE_PRIVATE);
        editor = sharedPref.edit();

        //get intent
        Intent intent = getIntent();

        debugText = (TextView) findViewById(R.id.textViewDebug);

        //assign EditText
        ssidText = (EditText) findViewById(R.id.editTextSSID);
        passwordText = (EditText) findViewById(R.id.editTextPassword);
        blindNickname = (EditText) findViewById(R.id.editTextNickname);

        //assign button
        connect_button = (Button) findViewById(R.id.buttonConnect);

        //set on click listener
        connect_button.setOnClickListener(this);
    }

    public void onClick(View view) {
        switch(view.getId()) {
            case R.id.buttonConnect:
                //get ssid and password
                //String networkSSID = ssidText.getText().toString().trim();
                //String networkPASS = passwordText.getText().toString().trim();

                //run connect to new blinds protocol
                //wifiConnect();

                //set new IP and new Port
                String newIP = "192.168.1.105";
                int newPORT = 6969;

                //run setup protocol
                //setupProtocol(newIP, newPORT, networkSSID, networkPASS);

                //get nickname
                String nickname = blindNickname.getText().toString();
                debugText.setText("Nickname: "+nickname);

                //build new blind object
                Blinds newBlind = new Blinds(newIP, newPORT, nickname);

                //save new blind data
                Gson gson = new Gson();
                String json = gson.toJson(newBlind);
                editor.putString(nickname, json);
                editor.commit();

                //add calibration step here

                break;
        }
    }

    public void wifiConnect() {
        WifiConfiguration conf = new WifiConfiguration();

        conf.SSID = "\""+ESP_SSID+"\""; //configure ssid

        /*for WEP network
        conf.wepKeys[0] = "\""+pass+"\"";   //configure password
        conf.wepTxKeyIndex = 0;
        conf.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
        conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP40);
        */

        //for WPA network
        conf.preSharedKey = "\""+ESP_PASS+"\"";   //configure password
        conf.status = WifiConfiguration.Status.ENABLED;

        //add to Android wifi manager settings
        WifiManager wifi = (WifiManager)this.getSystemService(Context.WIFI_SERVICE);
        int netid = wifi.addNetwork(conf);
        wifi.enableNetwork(netid, true);
        wifi.reconnect();

        //might need to enable--add code here to enable--
    }

    public void setupProtocol(String newIP, int newPORT, String localSSID, String localPASS) {
        try {
            socket = new Socket();
            socket.connect(new InetSocketAddress(ipAddress, portNumber), 300);

            // print writer
            PrintWriter out = new PrintWriter(new BufferedWriter(
                    new OutputStreamWriter(socket.getOutputStream())), true);
            // buffered reader
            BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));

            //send commands
            boolean sendSSID = sendCommand(out, in, "@strings/send_ssid_command"+localSSID);
            boolean sendPASS = sendCommand(out, in, "@strings/send_pass_command"+localPASS);
            boolean setIP = sendCommand(out, in, "@strings/set_ip_command"+newIP);
            boolean setPORT = sendCommand(out, in, "@strings/set_port_command"+newPORT);
            boolean sendRESET = sendCommand(out, in, "@strings/send_reset_command");

            socket.close();

        }catch (IOException e1) {
            e1.printStackTrace();
        }
    }

    public boolean sendCommand(PrintWriter output, BufferedReader input, String command) {
        boolean passed = true; // boolean for passing the command exchange
        //check protocol
        try {
            output.println(command);
            String response = input.readLine();
            if (!response.equals(command)) {
                passed = false;
            }
            output.println("@strings/okay_command");
            response = input.readLine();
            if (!response.equals("@strings/okay_command")){
                passed = false;
            }
        }catch (IOException e1){
            e1.printStackTrace();
        }

        return passed;
    }

    /*
    public String getNickname() {

        //get prompt.xml
        LayoutInflater li = LayoutInflater.from(this);
        View promptsView = li.inflate(R.layout.blind_name_prompt, null);

        AlertDialog.Builder adb = new AlertDialog.Builder(this);
        adb.setView(promptsView);
        final EditText userInput = (EditText) promptsView.findViewById(
                R.id.editTextBlindsNickname);

        // set dialog message
        adb
                .setCancelable(false)
                .setPositiveButton("Submit",
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int id) {
                                tempNickname = userInput.getText().toString();
                            }
                        })
                .setNegativeButton("Cancel",
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog,int id) {
                                tempNickname = "stupid default nickname";
                                dialog.cancel();
                            }
                        });

        AlertDialog alertDialog = adb.create();
        alertDialog.show();

        return tempNickname;
    }*/
}
