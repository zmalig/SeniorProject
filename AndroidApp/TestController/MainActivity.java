package com.librarian.zachary.blindscontroller;

import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity implements View.OnClickListener{
    public static int COUNT = 0;
    public final static String EXTRA_MESSAGE  = "nonsense";
    public static int tilt_value = 0;
    public static int pos_value = 0;


    ImageButton button_tiltUp, button_tiltDown, button_posUp, button_posDown;
    TextView tilt_text, pos_text;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        //image buttons (controller buttons)
        button_tiltUp = (ImageButton) findViewById(R.id.button_tiltUp);
        button_tiltUp.setOnClickListener(this);
        button_tiltDown = (ImageButton) findViewById(R.id.button_tiltDown);
        button_tiltDown.setOnClickListener(this);
        button_posUp = (ImageButton) findViewById(R.id.button_posUp);
        button_posUp.setOnClickListener(this);
        button_posDown = (ImageButton) findViewById(R.id.button_posDown);
        button_posDown.setOnClickListener(this);

        tilt_text = (TextView) findViewById(R.id.v_tilt);
        tilt_text.setText("Tilt: "+tilt_value);
        pos_text = (TextView) findViewById(R.id.v_pos);
        pos_text.setText("Position: "+pos_value);
    }

    @Override
    public void onClick(View view) {
        switch(view.getId()) {
            //case by case code
            case R.id.button_tiltUp:
                //run tilt up
                tilt_value++;
                tilt_text = (TextView) findViewById(R.id.v_tilt);
                tilt_text.setText("Tilt: "+tilt_value);
                break;

            case R.id.button_tiltDown:
                //run tilt down
                tilt_value--;
                tilt_text = (TextView) findViewById(R.id.v_tilt);
                tilt_text.setText("Tilt: "+tilt_value);
                break;

            case R.id.button_posUp:
                //run position up
                pos_value++;
                pos_text = (TextView) findViewById(R.id.v_pos);
                pos_text.setText("Position: "+pos_value);
                break;

            case R.id.button_posDown:
                //run position down
                pos_value--;
                pos_text = (TextView) findViewById(R.id.v_pos);
                pos_text.setText("Position: "+pos_value);
                break;

        }
    }

    //connect button method
    public void runConnect(View view) {
        Intent intent = new Intent(this, ConnectMenu.class);
        COUNT++;
        String message = "Got here: "+COUNT;
        intent.putExtra(EXTRA_MESSAGE, message);
        startActivity(intent);
    }
}
