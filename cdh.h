/*File for custom functions to control Chinese Diesel Heaters
    These functions expect to use ESPHome cc1101 module to connect to the heater
    the packets should be variable length which cuts off the leading byte
    */


uint8_t packet_seq; //Packet sequence number

/*
 * CRC-16/MODBUS
 */
uint16_t crc16_2(uint8_t *buf, int len) {
    //CRC code grabbed from https://github.com/jakkik/DieselHeaterRF

  uint16_t crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint8_t)buf[pos];
    for (int i = 8; i != 0; i--) {    
      if ((crc & 0x0001) != 0) {      
        crc >>= 1;                    
        crc ^= 0xA001;
      } else {                    
        crc >>= 1;
      }
    }
  }
  return crc;
}

void tx_wakeup(){
    /*Transmit wakeup packet to heater - Sould respond with status packet
    TODO add address handling
    Note: packet length (0x09) added to first byte of packet by radio
    */


    uint8_t data[9]; //Array containing packet
    uint8_t crc_data[10]; //packet with leading byte added


    data[0] = 0x23; //Wakeup packet type
    data[1] = 0x6D; //Address
    data[2] = 0xC3; //Address
    data[3] = 0x5C; //Address
    data[4] = 0x0D; //Address
    data[5] = packet_seq; //packet sequence
    data[8] = 0x00;

    packet_seq = packet_seq + 1;

    //Add first byte to packet for calculating CRC
    crc_data[0] = 0x09;
    for (int i = 0; i < 9; i++){
        crc_data[i+1] = data[i];
    }

    uint16_t crc = crc16_2(crc_data,7);

    data[6] = (crc >> 8) & 0xFF; //crc1
    data[7] = crc & 0xFF; //crc2

    //Log the CRC output
    ESP_LOGV("cdh", "crc: %X:%X", data[6], data[7]);
    //Pass packet details to log
    char hex_bufa[sizeof(data)*3];
    format_hex_pretty_to(hex_bufa, data, sizeof(data));
    ESP_LOGV("cdh", "Data Packet: %s", hex_bufa);
    char hex_bufb[sizeof(crc_data)*3];
    format_hex_pretty_to(hex_bufb, crc_data, sizeof(crc_data));
    ESP_LOGV("cdh", "CRC  Packet: %s", hex_bufb);


    //Tx Packet
    id(radio).transmit_packet(std::vector<uint8_t>(data, data + 9));
    //TODO use vector through full function

}

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