/*!
*****************************************************************
* seneka_dgps.cpp
*
* Copyright (c) 2013
* Fraunhofer Institute for Manufacturing Engineering
* and Automation (IPA)
*
*****************************************************************
*
* Repository name: seneka_sensor_node
*
* ROS package name: seneka_dgps
*
* Author: Ciby Mathew, E-Mail: Ciby.Mathew@ipa.fhg.de
* 
* Supervised by: Christophe Maufroy
*
* Date of creation: Jan 2013
* Modified 03/2014: David Bertram, E-Mail: davidbertram@gmx.de
* Modified 04/2014: Thorsten Kannacher, E-Mail: Thorsten.Andreas.Kannacher@ipa.fraunhofer.de
*
* Description:
*
* To-Do:
*
* - Generation and publishing of error messages
* - Extract all fields of a position record message (especially dynamic length of sat-channel_numbers and prns...)
* - Publish all gps values to ros topic (maybe need a new message if navsatFix cannot take all provided values...)
*
* - Monitor frequency/quality/... of incoming data packets... --> inform ROS about bad settings (publishing rate <-> receiving rate)
*
* - Rewrite function structure of interpretData and connected functions.. (still in dev state... double check for memory leaks etc...!!)
*
* - Extracting multi page messages from buffer...  (not needed for position records)
* - Clean up SerialIO files
* - Add more parameter handling (commandline, ...); document parameters and configuration
* - Testing!
*
*****************************************************************
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* - Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer. \n
* - Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution. \n
* - Neither the name of the Fraunhofer Institute for Manufacturing
* Engineering and Automation (IPA) nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission. \n
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License LGPL as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License LGPL for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License LGPL along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*
****************************************************************/

/*****************************************/
/***** DgpsNode class implementation *****/
/*****************************************/

#include <seneka_dgps/DgpsNode.h>

// constructor
DgpsNode::DgpsNode()
{
    //default parameters
    position_topic      = "/position";
    diagnostics_topic   = "/diagnostics";
    serial_port         = "/dev/ttyUSB0";
    serial_baudrate     = 38400;            // [] = Bd
    publishrate         = 1;                // [] = Hz

    nh = ros::NodeHandle("~");

    // get and set parameters from the ROS parameter server
    // if there is no matching parameter on the server, a default value is used

    // port
    if (!nh.hasParam("port"))
    {
        ROS_WARN("Using default parameter for port: %s", getSerialPort().c_str());
        publishStatus("Using default parameter for port.", 0);

    }
    nh.param("port", port, getSerialPort());

    // baud rate
    if (!nh.hasParam("baud"))
    {
        ROS_WARN("Using default parameter for baud rate: %i Bd", getSerialBaudRate());
        publishStatus("Using default parameter for baud rate.", 0);
    }    
    nh.param("baud", baud, getSerialBaudRate());

    // ROS publish rate
    if (!nh.hasParam("rate"))
    {
        ROS_WARN("Using default parameter for publish rate: %i Hz", getPublishRate());
        publishStatus("Using default parameter for publish rate.", 0);
    }
    nh.param("rate", rate, getPublishRate());

    syncedROSTime = ros::Time::now();

    topicPub_position       = nh.advertise<sensor_msgs::NavSatFix>              (position_topic.c_str(), 1);
    topicPub_Diagnostic_    = nh.advertise<diagnostic_msgs::DiagnosticArray>    (diagnostics_topic.c_str(), 1);
}

// destructor
DgpsNode::~DgpsNode(){}

// publishing functions

void DgpsNode::publishPosition(Dgps::gps_data gps)
{
    sensor_msgs::NavSatFix positions;

    positions.latitude          = gps.latitude_value;
    positions.longitude         = gps.longitude_value;
    positions.altitude          = gps.altitude_value;
    positions.header.frame_id   = "dgps_frame_id";
    positions.header.stamp      = ros::Time::now();

    topicPub_position.publish(positions);
}

void DgpsNode::publishStatus(std::string status_str, int level)
{
    diagnostic_msgs::DiagnosticArray diagnostics;

    diagnostics.status.resize(1);
    diagnostics.status[0].level     = level;
    diagnostics.status[0].name      = nh.getNamespace();
    diagnostics.status[0].message   = status_str;
    diagnostics.header.frame_id     = "dgps_frame_id";
    diagnostics.header.stamp        = ros::Time::now();

    topicPub_Diagnostic_.publish(diagnostics);
}

/****************************************/
/***** main program Seneka_dgps.cpp *****/
/****************************************/

// main program includes
#include <seneka_dgps/Dgps.h>

int main(int argc, char** argv)
{
    // ROS initialization and specification of node name
    ros::init(argc, argv, "DPGS");

    DgpsNode        cDgpsNode;
    Dgps            cDgps;
    Dgps::gps_data  position_record;

    bool dgpsSensor_opened      = false;
    bool connection_OK          = false;
    bool success_getPosition    = false;

    while (!dgpsSensor_opened)
    {
        ROS_INFO("Establishing connection to DGPS device... (Port: %s, Baud rate: %i)", cDgpsNode.getPort().c_str(), cDgpsNode.getBaud());
        cDgpsNode.publishStatus("Establishing connection to DGPS device...", 0);
        dgpsSensor_opened = cDgps.open(cDgpsNode.getPort().c_str(), cDgpsNode.getBaud());

        if (!dgpsSensor_opened)
        {
            ROS_ERROR("Connection to DGPS device failed. Device is not available on given port %s. Retrying to establish connection every second...", cDgpsNode.getPort().c_str());
            cDgpsNode.publishStatus("Connection to DGPS failed. DGPS is not available on given port. Retrying to establish connection every second...", 2);
        }
        else
        {
            ROS_INFO("Successfully connected to DPGS device.");
            cDgpsNode.publishStatus("Successfully connected to DPGS device.", 0);
        }

        // in case of success, wait for DPGS to get ready
        // in case of an error, wait before retry connection
        sleep(1);
    }

    ros::Rate loop_rate(cDgpsNode.getRate()); // [] = Hz

    // testing the communications link by sending protocol request ENQ (05h) (see BD982 manual, page 65)
    connection_OK = cDgps.checkConnection();

    if (!connection_OK)
    {
        ROS_ERROR("Testing the communications link failed (see Trimble BD982 GNSS Receiver manual page 65).");
        cDgpsNode.publishStatus("Testing the communications link failed (see Trimble BD982 GNSS Receiver manual page 65).", 2);
    }

    else
    {
        ROS_INFO("Successfully tested the communications link.");
        cDgpsNode.publishStatus("Successfully tested the communications link.", 0);

        ROS_INFO("Beginnig to obtain and publish DGPS data on topic %s...", cDgpsNode.getPositionTopic().c_str());
        cDgpsNode.publishStatus("Beginnig to obtain and publish DGPS data...", 0);

        /*****************************/
        /***** main program loop *****/
        /*****************************/

        while (cDgpsNode.nh.ok())
        {
            /*  call getPosition on dgps instance:
            *
            *   - requests position record from receiver
            *   - appends incoming data to ringbuffer
            *   - tries to extract valid packets (incl. checksum verification)
            *   - tries to read "Position Record"-fields from valid packets
            *   - writes "Position Record"-data into struct of type gps_data
            */
            success_getPosition = cDgps.getPosition(position_record);

            // publish GPS data to ROS topic if getPosition() was successfull, if not just publish status
            if (success_getPosition)
            {

                ROS_DEBUG("Successfully obtained DGPS data.");
                cDgpsNode.publishStatus("Successfully obtained DGPS data.", 0);

                ROS_DEBUG("Publishing DGPS position on topic %s...", cDgpsNode.getPositionTopic().c_str());
                cDgpsNode.publishStatus("Publishing DGPS position...", 0);
                
                cDgpsNode.publishPosition(position_record);
            }
            else
            {
                ROS_WARN("Failed to obtain DGPS data. No DGPS data available.");
                cDgpsNode.publishStatus("Failed to obtain DGPS data. No DGPS data available.", 1);
            }

            ros::spinOnce();
            loop_rate.sleep();
        }
    }

    return 0;
}

#ifdef DEBUG

// ##########################################################################
// ## dev-methods -> can be removed when not needed anymore                ##
// ##########################################################################

// context of this function needs to be created!!
//bool getFakePosition(double* latt) {
//    // set to true after extracting position values. method return value.
//    bool success = false;
//
//    int length;
//    unsigned char Buffer[1024] = {0};
//    int buffer_index = 0;
//    unsigned char data_buffer[1024] = {0};
//    int data_index = 0;
//    for (int i = 0; i < 1024; i++) Buffer[i] = '0';
//    char str[10];
//    char binary[10000] = {0};
//    int value[1000] = {0};
//    int open, y, bytesread, byteswrite, bin;
//
//    // see page 73 in BD982 user guide for packet specification
//    //  start tx,
//    //      status,
//    //          packet type,
//    //              length,
//    //                  type raw data,      [0x00: Real-Time Survey Data Record; 0x01: Position Record]
//    //                      flags,
//    //                          reserved,
//    //                              checksum,
//    //                                  end tx
//    unsigned char stx_ = 0x02;
//    unsigned char status_ = 0x00;
//    unsigned char packet_type_ = 0x56;
//    unsigned char length_ = 0x03;
//    unsigned char data_type_ = 0x01;
//    unsigned char etx_ = 0x03;
//
//    unsigned char checksum_ = status_ + packet_type_ + data_type_ + length_;
//    //(status_ + packet_type_ + data_type_ + 0 + 0 + length_)%256;
//
//
//
//    char message[] = {stx_, status_, packet_type_, length_, data_type_, 0x00, 0x00, checksum_, etx_}; // 56h command packet       // expects 57h reply packet (basic coding)
//
//    //        char message[]={ 0x05 };
//    length = sizeof (message) / sizeof (message[0]);
//
//    cout << "length of command: " << length << "\n";
//
//    //SerialIO dgps;
//    //open = dgps.open();
//    byteswrite = 9; //m_SerialIO.write(message, length);
//    printf("Total number of bytes written: %i\n", byteswrite);
//    std::cout << "command was: " << std::hex << message << "\n";
//    sleep(1);
//    bytesread = 118; //m_SerialIO.readNonBlocking((char*) Buffer, 1020);
//
//    string test_packet = " |02|  |20|  |57|  |0a|  |0c|  |11|  |00|  |00|  |00|  |4d|  |01|  |e1|  |01|  |e1|  |af|  |03|  |02|  |20|  |57|  |60|  |01|  |11|  |00|  |00|  |3f|  |d1|  |54|  |8a|  |b6|  |cf|  |c6|  |8d|  |3f|  |a9|  |e0|  |bd|  |3f|  |29|  |c8|  |f7|  |40|  |80|  |ae|  |2a|  |c9|  |7b|  |b7|  |11|  |c0|  |fd|  |d3|  |79|  |61|  |fb|  |23|  |99|  |c0|  |92|  |ca|  |3b|  |46|  |c7|  |05|  |15|  |3f|  |ff|  |9f|  |23|  |e0|  |00|  |00|  |00|  |be|  |44|  |16|  |1f|  |0d|  |84|  |d5|  |33|  |be|  |2c|  |8b|  |3b|  |bb|  |46|  |eb|  |85|  |3f|  |b5|  |ec|  |f0|  |c0|  |00|  |00|  |00|  |08|  |06|  |f0|  |d8|  |d4|  |07|  |0f|  |0d|  |13|  |01|  |0c|  |07|  |04|  |07|  |0d|  |08|  |09|  |0a|  |1a|  |1c|  |5c|  |03|";
//           test_packet = " |02|  |20|  |57|  |0a|  |0c|  |11|  |00|  |00|  |00|  |4d|  |01|  |e1|  |01|  |e1|  |af|  |03|  |02|  |20|  |57|  |60|  |01|  |11|  |00|  |00|  |3f|  |d1|  |54|  |8a|  |b6|  |cf|  |c6|  |8d|  |3f|  |a9|  |e0|  |bd|  |3f|  |29|  |c8|  |f7|  |40|  |80|  |ae|  |2a|  |c9|  |7b|  |b7|  |11|  |c0|  |fd|  |d3|  |79|  |61|  |fb|  |23|  |99|  |c0|  |92|  |ca|  |3b|  |46|  |c7|  |05|  |15|  |3f|  |ff|  |9f|  |23|  |e0|  |00|  |00|  |00|  |be|  |44|  |16|  |1f|  |0d|  |84|  |d5|  |33|  |be|  |2c|  |8b|  |3b|  |bb|  |46|  |eb|  |85|  |3f|  |b5|  |ec|  |f0|  |c0|  |00|  |00|  |00|  |08|  |06|  |f0|  |d8|  |d4|  |07|  |0f|  |0d|  |13|  |01|  |0c|  |07|  |04|  |07|  |0d|  |08|  |09|  |0a|  |1a|  |1c|  |5c|  |03|";
//
//    for (int i = 0; i < bytesread; i++) {
//        char hex_byte1 = test_packet[i * 6 + 2];
//        char hex_byte2 = test_packet[i * 6 + 3];
//
//        if (hex_byte1 > 96) hex_byte1 -= 87; // 96-9
//        else hex_byte1 -= 48;
//        if (hex_byte2 > 96) hex_byte2 -= 87; // 96-9
//        else hex_byte2 -= 48;
//
//        Buffer[i] = hex_byte1 * 16 + hex_byte2;
//        printf("%x%x-%i  ", hex_byte1, hex_byte2, Buffer[i]);
//
//    }
//    cout << "\n";
//
//    printf("\nTotal number of bytes read: %i\n", bytesread);
//    cout << "-----------\n";
//    for (int i = 0; i < bytesread; i++) {
//        printf(" |%.2x| ", Buffer[buffer_index + i]);
//
//    }
//    cout << std::dec << "\n";
//
//    cout << "-----------\n";
//
//    packet_data incoming_packet;
//    Dgps temp_gps_dev = Dgps();
//    temp_gps_dev.interpretData(Buffer, bytesread, incoming_packet);
//
//
//    // need to check if values were ok, right now just hardcoded true..
//    success = true;
//    return success;
//}

#endif