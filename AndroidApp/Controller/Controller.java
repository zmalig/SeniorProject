package com.miniblinds.controller;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
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
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class Controller extends AppCompatActivity implements View.OnClickListener, AdapterView.OnItemSelectedListener{

    //constants
    private SharedPreferences sharedPref;
    private SharedPreferences.Editor editor;
    private TextView debugText;

    private Button  posUp, posDown, tiltUp, tiltDown;
    private TextView posText, tiltText;

    private Spinner blindSelect;
    private Socket socket;

    private String ipAddress;
    private int portNumber, currentTilt, currentPos;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_controller);

        //get intent
        Intent intent = getIntent();

        //get shared preferences
        sharedPref = getSharedPreferences("com.controller.BLINDS_PREFS", Context.MODE_PRIVATE);
        editor = sharedPref.edit();

        debugText = (TextView) findViewById(R.id.textViewDebug);

        //initialize buttons
        posUp = (Button) findViewById(R.id.buttonPosUp);
        posDown = (Button) findViewById(R.id.buttonPosDown);
        tiltUp = (Button) findViewById(R.id.buttonTiltUp);
        tiltDown = (Button) findViewById(R.id.buttonTiltDown);

        //initialize text view
        posText = (TextView) findViewById(R.id.textViewPos);
        tiltText = (TextView) findViewById(R.id.textViewTilt);

        //set on click listeners
        posUp.setOnClickListener(this);
        posDown.setOnClickListener(this);
        tiltUp.setOnClickListener(this);
        tiltDown.setOnClickListener(this);

        addBlindsSpinner();
    }

    public void onClick(View view) {
        try {
            socket = new Socket();
            socket.connect(new InetSocketAddress(ipAddress, portNumber), 300);

            // print writer
            PrintWriter out = new PrintWriter(new BufferedWriter(
                    new OutputStreamWriter(socket.getOutputStream())), true);
            // buffered reader
            BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));

            //send commands on button presses
            switch(view.getId()) {
                case R.id.buttonPosUp:
                    sendCommand(out, in, "@strings/pos_up_command");
                    break;
                case R.id.buttonPosDown:
                    sendCommand(out, in, "@strings/pos_down_command");
                    break;
                case R.id.buttonTiltUp:
                    sendCommand(out, in, "@strings/tilt_up_command");
                    break;
                case R.id.buttonTiltDown:
                    sendCommand(out, in, "@strings/tilt_down_command");
                    break;
            }

            socket.close();
        } catch (IOException e1) {
            e1.printStackTrace();
        }
    }

    public void addBlindsSpinner(){

        blindSelect = (Spinner) findViewById(R.id.spinnerBlindsList);   //initialize spinner
        blindSelect.setOnItemSelectedListener(this);    //set listener

        //get blind names
        List<String> blindNamesList = new ArrayList<String>();  //blind name list
        Map<String, ?> allEntries = sharedPref.getAll();
        for(Map.Entry<String, ?> entry : allEntries.entrySet()) {
            String keyValue = entry.getKey();
            blindNamesList.add(keyValue);
        }

        //create adapter for spinner
        ArrayAdapter<String> dataAdapter = new ArrayAdapter<String>(this,
                android.R.layout.simple_spinner_item, blindNamesList);

        //drop down layout style
        dataAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

        //attaching data adapter to spinner
        blindSelect.setAdapter(dataAdapter);
    }

    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        //on selected spinner item
        String selectedBlindName = parent.getItemAtPosition(position).toString();
            //get blind info
            Gson gson = new Gson();
            String json = sharedPref.getString(selectedBlindName, "");
            Blinds currentBlind = gson.fromJson(json, Blinds.class);

            //get saved values from blinds object
            ipAddress = currentBlind.getIpAddress();
            portNumber = currentBlind.getPortNumber();
            currentPos = currentBlind.getPosition();
            currentTilt = currentBlind.getTilt();
            debugText.setText(ipAddress);
    }
    public void onNothingSelected(AdapterView<?> arg0) {
        //do nothing
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

}
