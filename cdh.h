/*File for custom functions to control Chinese Diesel Heaters
    These functions expect to use ESPHome cc1101 module to connect to the heater
    the packets should be variable length which cuts off the leading byte
    */



uint32_t extract_address(uint8_t a, uint8_t b, uint8_t c, uint8_t d){
    //Arguments are 1 byte each of the address as found in the packet
    
    uint32_t addr = 0;

    addr |= (a << 24);
    addr |= (b << 16);
    addr |= (c << 8);
    addr |= (d);

    //ESP_LOGD("cdh", "Addr is: %X", addr);

    return addr;
}

void read_state(const uint8_t data[100]){
    /*Take the full packet as sent from the radio, interpret it and 
        send the latest values to the internal variables
        Byte 0 is the packet type
        Bytes 1-4 are the address
        */
    
    //Get status bytes out of the packet
    uint8_t state = data[5];
    uint8_t power = data[6];
    float voltage = data[8] / 10.0f;
    int8_t ambientTemp = data[9];
    uint8_t caseTemp = data[11];
    int8_t setPoint = data[12];
    uint8_t autoMode = data[15];
    float pumpFrequency = data[14] / 10.0f;
    
    //Push values to outputs
    id(waterTempI).publish_state(caseTemp);
    id(ambientTempI).publish_state(ambientTemp);
    id(pumpFrequencyI).publish_state(pumpFrequency);
    id(setPointI).publish_state(setPoint);
}

void read_packet(const uint8_t packet[100]){//TODO check needed packet length
    /* Take full packet from radio and decide what to do with it
    */

    //Get address into a useful format
    uint32_t address = extract_address(packet[1], packet[2], packet[3], packet[4]);

    
          
    if ( address == 0x6DC35C0D){
        
            
        //Check what type of packet has been recieved
        switch (packet[0]) {
            case 0x00:
                //Reporting heater conditions
                //read_state(packet.data());
                read_state(packet);
                break;
            case 0x23:
                //Heater wakeup
                break;
            case 0x24:
                // CMD Mode
                break;
            case 0x2b:
                // CMD Power
                break;
            case 0x3c:
                // CMD Up
                break;
            case 0x3e:
                // CMD Down
                break;
            default:
                //Unrecognized packet type
                ESP_LOGD("cc1101", "Packet with unknown type recieved");
            }

    } else {
        ESP_LOGD("cc1101", "Packet with unknown address recieved");
        //ESP_LOGD("cc1101", "unknown packet %s freq_offset %.0f Hz rssi %.1f dBm lqi %u",
            //format_hex_to(hex, x), freq_offset, rssi, lqi);
        }

}