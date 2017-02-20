package com.miniblinds.controller;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;

public class Home extends AppCompatActivity implements View.OnClickListener{

    //constants
    Button controller_menu, connect_menu, settings_menu;

    private SharedPreferences sharedPref;
    private SharedPreferences.Editor editor;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_home);

        //initialize buttons
        controller_menu = (Button) findViewById(R.id.buttonController);
        connect_menu = (Button) findViewById(R.id.buttonConnect);
        settings_menu = (Button) findViewById(R.id.buttonSettings);

        //set on click listener
        controller_menu.setOnClickListener(this);
        connect_menu.setOnClickListener(this);
        settings_menu.setOnClickListener(this);


        /*clear shared preferences
        sharedPref = getSharedPreferences("com.controller.BLINDS_PREFS", Context.MODE_PRIVATE);
        editor = sharedPref.edit();
        editor.clear();
        editor.commit();*/
    }

    //button clicks
    public void onClick(View view) {
        switch(view.getId()) {
            case R.id.buttonConnect:
                //connect menu
                Intent connect_intent = new Intent(this, Connect.class);
                startActivity(connect_intent);
                break;

            case R.id.buttonController:
                //controller menu
                Intent controller_intent = new Intent(this, Controller.class);
                startActivity(controller_intent);
                break;

            case R.id.buttonSettings:
                //settings menu

                break;
        }
    }
}

