package com.miniblinds.controller;

/**
 * Created by Zachary Malig on 2/13/2017.
 * Public Blinds Class with all the data for a blinds unit
 */

public class Blinds {
    private String ipAddress, name;
    private int position, tilt, portNumber;

    public Blinds(String ip, int p, String n) {
        ipAddress = ip;
        name = n;
        portNumber = p;
    }

    //get methods
    public String getIpAddress() {
        return ipAddress;
    }

    public String getName() {
        return name;
    }

    public int getPosition() {
        return position;
    }

    public int getTilt() {
        return tilt;
    }

    public int getPortNumber() {
        return portNumber;
    }

    //set methods
    public void setName(String n) {
        name = n;
    }

    public void setPosition(int pos) {
        position = pos;
    }

    public void setTilt(int t) {
        tilt = t;
    }


}
