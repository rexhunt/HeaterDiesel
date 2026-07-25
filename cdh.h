/*File for custom functions to control Chinese Diesel Heaters
    These functions expect to use ESPHome cc1101 module to connect to the heater
    the packets should be variable length which cuts off the leading byte
*/

//State Codes from https://github.com/jakkik/DieselHeaterRF
#define HEATER_STATE_OFF            0x00
#define HEATER_STATE_STARTUP        0x01
#define HEATER_STATE_WARMING        0x02
#define HEATER_STATE_WARMING_WAIT   0x03
#define HEATER_STATE_PRE_RUN        0x04
#define HEATER_STATE_RUNNING        0x05
#define HEATER_STATE_SHUTDOWN       0x06
#define HEATER_STATE_SHUTTING_DOWN  0x07
#define HEATER_STATE_COOLING        0x08

//Heater Command codes from https://github.com/jakkik/DieselHeaterRF
#define HEATER_CMD_WAKEUP 0x23
#define HEATER_CMD_MODE   0x24
#define HEATER_CMD_POWER  0x2b
#define HEATER_CMD_UP     0x3c
#define HEATER_CMD_DOWN   0x3e

uint8_t packet_seq; //Packet sequence number
char stateT[13]; //State in ASCII
uint8_t last_seq; //Last recieved sequence number
uint32_t myAddress = 0x6DC35C0D; //Address for this heater

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

void format_packet_print(char *arr, const uint8_t packet[100], int size){
    int io = 0;
    for (int i = 0; i < size; i++){
        snprintf(arr, 4, "%02X:", packet[i]);
        arr += 3;
    }
}

void tx_packet(uint8_t type){

    int length = 9; //Packet length
    uint8_t data[length]; //Array containing packet
    uint8_t crc_data[length + 1]; //packet with leading byte added

    // Packet Type
    data[0] = type;

    //Address       Right Shift and apply mask
    data[1] = (myAddress >> 3 * 8) & 0xFF;
    data[2] = (myAddress >> 2 * 8) & 0xFF;
    data[3] = (myAddress >> 1 * 8) & 0xFF;
    data[4] = (myAddress >> 0 * 8) & 0xFF;

    //Remaining bytes
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

    char hex[26*3];
    format_packet_print(hex, data, length);
    ESP_LOGV("cc1101", "tx packet %s ", hex);

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

void state_text(uint8_t state){
    uint8_t stss = 0x05;

    switch (state) {
        case 0x00:
            strncpy(stateT, "OFF", sizeof(stateT)-1);
            break;
        case 0x01:
            strncpy(stateT, "Startup", sizeof(stateT)-1);
            break;
        case 0x02:
            strncpy(stateT, "Warming", sizeof(stateT)-1);
            break;
        case 0x03:
            strncpy(stateT, "Warming Wait", sizeof(stateT)-1);
            break;
        case 0x04:
            strncpy(stateT, "Pre Run", sizeof(stateT)-1);
            break;
        case 0x05:
            strncpy(stateT, "Running", sizeof(stateT)-1);
            break;
        case 0x06:
            strncpy(stateT, "Shutdown", sizeof(stateT)-1);
            break;
        case 0x07:
            strncpy(stateT, "Shutting Down", sizeof(stateT)-1);
            break;
        case 0x08:
            strncpy(stateT, "Cooling", sizeof(stateT)-1);
            break;
        default:
            strncpy(stateT, "Unk", sizeof(stateT)-1);
    }

    return;
}

void read_state(const uint8_t data[100]){
    /*Take the full packet as sent from the radio, interpret it and 
        send the latest values to the internal variables
        Byte 0 is the packet type
        Bytes 1-4 are the address
    */
    
    char temp[50]; //Temp array for misc strings
    
    //Get status bytes out of the packet
    uint8_t state = data[5];
    uint8_t power = data[6];
    float voltage = data[8] / 10.0f;
    int8_t ambientTemp = data[9];
    uint8_t caseTemp = data[11];
    int8_t setPoint = data[12];
    uint8_t autoMode = data[15];
    float pumpFrequency = data[14] / 10.0f;
    uint8_t seq = data[19];

    state_text(state);
    
    //Push values to outputs
    id(stateI).publish_state(stateT);
    snprintf(temp, sizeof(temp), "%X", power);
    id(powerI).publish_state(temp);
    id(voltageI).publish_state(voltage);
    id(ambientTempI).publish_state(ambientTemp);
    id(waterTempI).publish_state(caseTemp);
    id(setPointI).publish_state(setPoint);
    snprintf(temp, sizeof(temp), "%X", autoMode);
    id(autoI).publish_state(temp);
    id(pumpFrequencyI).publish_state(pumpFrequency);
    snprintf(temp, sizeof(temp), "%X", seq);
    id(seqI).publish_state(temp);       //TODO check if we have recieved this packet before

    // Section for unknown bytes
    uint8_t byte7 = data[7];
    uint8_t byte10 = data[10];
    uint8_t byte13 = data[13];
    uint8_t byte16 = data[16];
    uint8_t byte17 = data[17];
    uint8_t byte18 = data[18];


    snprintf(temp, sizeof(temp), "%X", byte7);
    id(byteA).publish_state(temp);
    snprintf(temp, sizeof(temp), "%X", byte10);
    id(byteB).publish_state(temp);
    snprintf(temp, sizeof(temp), "%X", byte13);
    id(byteC).publish_state(temp);
    snprintf(temp, sizeof(temp), "%X", byte16);
    id(byteD).publish_state(temp);
    snprintf(temp, sizeof(temp), "%X", byte17);
    id(byteE).publish_state(temp);
    //snprintf(temp, sizeof(temp), "%d", byte18);
    id(byteF).publish_state(byte18);
}


bool dup_packet(const uint8_t packet[100],int posn){//packet, position in packet of sequence
    /*Check if this packet has already been recieved
    Return True if a duplicate packet, False if not
    */

    uint8_t new_seq = packet[posn]; //New recieved sequence number

    
    if (new_seq == last_seq){//This is a duplicate packet and should be dropped
            ESP_LOGV("cdh", "Dropping duplicate packet");
            return true;
    }

    ESP_LOGV("cdh", "Updating New seq = %X old seq = %X", new_seq, last_seq);
    last_seq = new_seq;

    //Log packet for identifying new bytes
    char hex[26*3];
    format_packet_print(hex, packet, posn + 6);
    ESP_LOGD("cc1101", "packet %s ", hex);

    return false;

}

void read_packet(const uint8_t packet[100]){//TODO check needed packet length
    /* Take full packet from radio and decide what to do with it
    */

    //Get address into a useful format
    uint32_t address = extract_address(packet[1], packet[2], packet[3], packet[4]);

    char hex[26*3];  // Less than *3 cuts the end off the packets
          
    if ( address == myAddress){
        
        
        //Check what type of packet has been recieved
        switch (packet[0]) {
            case 0x00:
                //Reporting heater conditions
                //read_state(packet.data());
                if(dup_packet(packet, 19)){break;}
                read_state(packet);
                break;
            case 0x23:
                //Heater wakeup
                if(dup_packet(packet, 5)){break;}
                ESP_LOGD("cdh", "Packet waking up heater recieved");
                break;
            case 0x24:
                // CMD Mode
                if(dup_packet(packet, 5)){break;}
                ESP_LOGD("cdh", "Packet cmd recieved");
                format_packet_print(hex, packet, 9);
                ESP_LOGD("cc1101", "rx packet %s ", hex);
                break;
            case 0x2b:
                // CMD Power
                if(dup_packet(packet, 5)){break;}
                ESP_LOGD("cdh", "Packet power recieved");
                format_packet_print(hex, packet, 9);
                ESP_LOGD("cc1101", "rx packet %s ", hex);
                break;
            case 0x3c:
                // CMD Up
                if(dup_packet(packet, 5)){break;}
                format_packet_print(hex, packet, 9);
                ESP_LOGD("cc1101", "rx packet %s ", hex);
                ESP_LOGD("cdh", "Packet Up recieved");
                break;
            case 0x3e:
                // CMD Down
                if(dup_packet(packet, 5)){break;}
                format_packet_print(hex, packet, 9);
                ESP_LOGD("cc1101", "rx packet %s ", hex);
                ESP_LOGD("cdh", "Packet down recieved");
                break;
            default:
                //Unrecognized packet type
                ESP_LOGD("cdh", "Packet with unknown type recieved");
                //Log packet for identifying new bytes
                format_packet_print(hex, packet, 25);
                ESP_LOGD("cc1101", "rx packet %s ", hex);
            }

    } else {
        ESP_LOGD("cc1101", "Packet with unknown address recieved");
        //Log packet for identifying new bytes
        format_packet_print(hex, packet, 25);
        ESP_LOGD("cc1101", "rx packet %s ", hex);
        //ESP_LOGD("cc1101", "unknown packet %s freq_offset %.0f Hz rssi %.1f dBm lqi %u",
            //format_hex_to(hex, x), freq_offset, rssi, lqi);
        }

}