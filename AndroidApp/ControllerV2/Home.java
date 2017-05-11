package com.miniblinds.smartminiblinds;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.StrictMode;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import com.google.gson.Gson;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class Home extends AppCompatActivity implements View.OnClickListener{

    //constants
    private SharedPreferences sharedPref;
    private SharedPreferences.Editor editor;

    Button controller_pos_up, controller_pos_down, controller_tilt_up, controller_tilt_down;
    Button menu_settings, menu_connect, menu_extra, buttonIsConnect;
    TextView textViewIsConnect;

    ListView blind_select;
    List<String> blindNamesList;

    Map<String, ?> allEntries;

    private String ipAddress;
    private int portNumber, currentTilt, currentPos;
    private boolean isConnected;

    Socket socket;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_home);
        setTitle("Smart Mini Blinds");

        //enable socket stuff
        if (android.os.Build.VERSION.SDK_INT > 9) {
            StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
            StrictMode.setThreadPolicy(policy);
        }

        //get Shared Preferences
        sharedPref = getSharedPreferences("com.smartminiblinds.BLIND_PREFS", Context.MODE_PRIVATE);
        editor = sharedPref.edit();

        //initialize constants
        controller_pos_up = (Button) findViewById(R.id.buttonPositionUp);
        controller_pos_down = (Button) findViewById(R.id.buttonPositionDown);
        controller_tilt_up = (Button) findViewById(R.id.buttonTilt1);
        controller_tilt_down = (Button) findViewById(R.id.buttonTilt2);
        menu_settings = (Button) findViewById(R.id.buttonSettings);
        menu_connect = (Button) findViewById(R.id.buttonAddBlind);
        menu_extra = (Button) findViewById(R.id.buttonExtra);

        textViewIsConnect = (TextView) findViewById(R.id.textViewIsConnect);

        blind_select = (ListView) findViewById(R.id.listViewBlindSelect);

        //connection_status = (TextView) findViewById(R.id.textViewConnectStatus);

        //set click listeners
        controller_pos_up.setOnClickListener(this);
        controller_pos_down.setOnClickListener(this);
        controller_tilt_up.setOnClickListener(this);
        controller_tilt_down.setOnClickListener(this);
        menu_settings.setOnClickListener(this);
        menu_connect.setOnClickListener(this);
        menu_extra.setOnClickListener(this);

        //Initialize Blind Select

        //get blind names
        blindNamesList = new ArrayList<>();  //blind name list
        allEntries = sharedPref.getAll();
        for(Map.Entry<String, ?> entry : allEntries.entrySet()) {
            String keyValue = entry.getKey();
            blindNamesList.add(keyValue);
        }

        //propagate ListView
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, R.layout.activity_blindselectlistview, blindNamesList);
        blind_select.setAdapter(adapter);

        //set listener
        blind_select.setOnItemClickListener(new ListClickHandler());

        isConnected = false;
    }

    public void onClick(View view) {
        switch(view.getId()) {
            // CONTROLLER COMMANDS //
            case R.id.buttonPositionUp:
                //position up
                sendCommand("P|U|1|0|0");
                break;
            case R.id.buttonPositionDown:
                //position down
                sendCommand("P|D|1|0|0");
                break;
            case R.id.buttonTilt1:
                //tilt up
                sendCommand("T|U|0|0|1");
                break;
            case R.id.buttonTilt2:
                //tilt down
                sendCommand("T|D|1|0|1");
                break;

            // MENU COMMANDS //
            case R.id.buttonSettings:
                //setting menu
                break;
            case R.id.buttonExtra:
                //extra menu
                break;
            case R.id.buttonAddBlind:
                //add blind menu
                Intent connect_intent = new Intent(this, Connect.class);
                startActivity(connect_intent);
                finish();
                break;
        }

    }

    public void sendCommand (String command) {
        try {
            String dataValue = "{"+command+"}";
            //writer
            PrintWriter out =
                    new PrintWriter(socket.getOutputStream(), true);
            //reader
            BufferedReader in =
                    new BufferedReader(
                            new InputStreamReader(socket.getInputStream()));
            BufferedReader stdIn =
                    new BufferedReader(
                            new InputStreamReader(System.in));
            out.println(dataValue);
            out.flush();
            Log.i("Thread Tag", "Success!");    // declare success
            //String response = stdIn.readLine();
            //Log.i("Server Response", response);
        } catch (IOException e) {
            e.printStackTrace();
            runToast("Oops!");
            Log.i("Status", "Failure");
        } catch (NumberFormatException e) {
            e.printStackTrace();
            runToast("Uh Oh Spaghetti-O!");
            Log.i("Status", "Failure");
        } catch (NullPointerException e) {
            e.printStackTrace();
            runToast("Something Went Wrong");
            Log.i("Status", "Failure");
        }
    }

    public class ListClickHandler implements AdapterView.OnItemClickListener {

        public void onItemClick(AdapterView<?> l, View v, int position, long id) {
            Log.i("HelloListView", "You clicked Item: " + id + " at position: " + position);
            // Start Client Thread
            String selectedName = blindNamesList.get(position);

            //get Blind Info
            Gson gson = new Gson();
            String json = sharedPref.getString(selectedName, "");
            Blind currentBlind = gson.fromJson(json, Blind.class);
            ipAddress = currentBlind.getIpAddress();
            portNumber = currentBlind.getPortNumber();

            ClientThread clientThread = new ClientThread(com.miniblinds.smartminiblinds.Home.this);
            clientThread.execute();

        }

    }


    //
    public class ClientThread extends AsyncTask<String, Void, Integer> {

        private Context mContext;

        public ClientThread(Context context) {
            mContext = context.getApplicationContext();
        }

        @Override
        protected void onPreExecute() {
            Log.i("Client Thread", "Starting Client Thread");
            textViewIsConnect.setBackgroundColor(Color.RED);
        }

        @Override
        protected Integer doInBackground(String... params) {
            try {
                InetAddress serverAddr = InetAddress.getByName(ipAddress);
                socket = new Socket(serverAddr, portNumber);
                if(socket.isConnected()){
                    return 1;
                }
                return 1;
            } catch (UnknownHostException e1) {
                e1.printStackTrace();
            } catch (IOException e1) {
                e1.printStackTrace();
            }
            return -1;
        }

        @Override
        protected void onPostExecute(Integer integer) {
            if (integer != -1) {
                Log.i("Client Thread", "Completed Client Thread");
                textViewIsConnect.setBackgroundColor(Color.GREEN);
            }
            else {
                runToast("Unable To Connect");
            }
        }
    }


    public void runToast(String text) {
        Context context = getApplicationContext();
        int duration = Toast.LENGTH_SHORT;
        Toast toast = Toast.makeText(context, text, duration);
        toast.show();
    }

}
