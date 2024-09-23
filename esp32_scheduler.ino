#include <NimBLEDevice.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string>
#include <Arduino.h>
#include <iostream>
#include <math.h>

// UUIDs for the service and characteristic
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"                  // Custom Service UUID
#define BULB_NAME_CHARACTERISTIC_UUID "87654321-4321-4321-4321-210987654321"        // Custom Characteristic UUID for ble naming
#define SUNSET_SETTINGS_CHARACTERISTIC_UUID "f00d4a3e-b447-49d9-b97c-4d7b0360c0ad"  // Custom Characteristic UUID for scheduling

NimBLEAdvertising *pAdvertising;

const char* defaultBulbName = "Client_device";
char currentBulbName[50] = "";
int currentTemperatureValue = 3000;
int currentBrightnessValue = 50;

esp_err_t err = nvs_flash_init();
/*
if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
}
*/
nvs_handle_t my_handle;

// this is my time library
struct SimpleTime {
  int hour;
  int minute;

  // Subtract two SimpleTime values, assuming both are on the same day
  SimpleTime subtract(SimpleTime other) {
    int totalMinutes1 = this->hour * 60 + this->minute;
    int totalMinutes2 = other.hour * 60 + other.minute;
    int diff = abs(totalMinutes1 - totalMinutes2); // Get the absolute difference in minutes
    SimpleTime result;
    result.hour = diff / 60;
    result.minute = diff % 60;
    return result;
  }
  
  // return the difference between two times in minutes
  int getRemainingMinutes(SimpleTime other) {   // format is (this - other)
    if((other.hour == 0) && (this->hour > 18)){
      int totalMinutes1 = this->hour * 60 + this->minute;
      int totalMinutes2 = 24 * 60 + other.minute;
      int diff = abs(totalMinutes1 - totalMinutes2); // Get the absolute difference in minutes
      SimpleTime result;
      result.hour = diff / 60;
      result.minute = diff % 60;
      return ((result.hour*60)+result.minute);
    }
    else{
      int totalMinutes1 = this->hour * 60 + this->minute;
      int totalMinutes2 = other.hour * 60 + other.minute;
      int diff = abs(totalMinutes1 - totalMinutes2); // Get the absolute difference in minutes
      SimpleTime result;
      result.hour = diff / 60;
      result.minute = diff % 60;
      return ((result.hour*60)+result.minute);
    }
  }

  // add some minutes to time
  SimpleTime addMinutes(int minutesToAdd) {
    int newMinutes = this->minute + minutesToAdd; // Add minutes directly
    int extraHours = newMinutes / 60; // Find how many hours are in the new minute total
    newMinutes %= 60; // Reduce the minute count to less than 60

    SimpleTime result;
    result.hour = (this->hour + extraHours) % 24; // Add the extra hours and roll over if needed
    result.minute = newMinutes;
    return result;
  }

  // check if both time same, return true if same
  bool checkTime(SimpleTime other){
    if( (this->hour == other.hour) && (this->minute == other.minute) ){
      return 1;
    }
    else{
      return 0;
    }
  }

  // the time calling this function should be bigger than argument time in order to return true
  bool isBig(SimpleTime other){
    
    int totalMinutes1 = this->hour * 60 + this->minute;   
    int totalMinutes2 = other.hour * 60 + other.minute;
    int diff = totalMinutes1 - totalMinutes2; // Get the absolute difference in minutes
    //Serial.println(this->hour);
    //Serial.println(other.hour);

    if ( (this->hour == 0) && (other.hour > 18)){  // problem with this method was that 00:23 is considered smaller than 23:58, hence this if statement is added to handle this scenario
      return 1;
    }
    else if (diff >= 0){      
      return 1;
    }
    else{
      return 0;
    }
  }

  void printTime(){
    if(this->hour < 10){
      Serial.print("0");
    }
    Serial.print(this->hour);
    Serial.print(":");
    if(this->minute < 10){
      Serial.print("0");
    }
    Serial.println(this->minute);
  }

  /*  this is helper code for SimpleTime, do not remove.
  SimpleTime diff = t2.subtract(t1);
  Serial.print("Time difference is ");
  Serial.print(diff.hour);
  Serial.print(" hours and ");
  Serial.print(diff.minute);
  Serial.println(" minutes.");
  */
};

SimpleTime globalTime = {6, 59}; // set the time as 06:00 initially
char* globalValue = nullptr;  // Global pointer, explanation in retrieve_string
unsigned long previousMillis = 0;
//std::string defaultBulbName = "Circa_Sleep";
char* globalString;           // used deep within code, because of info.key problem discussed in ListKeys function. Do not remove this variable.
int red, green, blue;



struct Node {
    char* data;       // holds the name of the schedule
    SimpleTime time;  // holds the time of the schedule
    Node* next;       // Pointer to the next node in the list
};

Node* head = NULL;    // Start with an empty list

void addNode(char* value, SimpleTime timeValue) {
    Node* newNode = new Node(); // Dynamically allocate a new node
    newNode->data = value;      
    newNode->time = timeValue;  
    newNode->next = head;       // Point the new node to the current head of the list
    head = newNode;             // Update head to the new node
}

void updateNode(char* value, SimpleTime timeValue){
  Node* temporaryIterator = head;
  while(temporaryIterator!=NULL){
    //Serial.print("checking ");
    //Serial.println(temporaryIterator->data);
    if( (temporaryIterator->data == value) || strcmp(value, temporaryIterator->data) == 0){    // this strcmp thing is frustrating.. wasted 1 hour..
      //Serial.println("name matched");
      temporaryIterator->time = timeValue;
      break;
      }
    temporaryIterator = temporaryIterator->next;
    }
}

// free up the linked list memory.. many elements are in heap, some are mallocly created.. it's a mess in linked list
void deleteNode(Node* node) {
    if (node != nullptr) {
        free(node->data); // Free the duplicated string
        delete node;      // Free the node itself
    }
}

// sort the linked list based on time.. this function does not work, will remove it
void sortNode(){
  int totalNodes = 0;
  Node* temporaryIterator = head;
  while(temporaryIterator!=NULL){
    totalNodes += 1;
    temporaryIterator = temporaryIterator->next;
  }
  Serial.print("total number of nodes are : ");
  Serial.println(totalNodes);

  for(int i=0;i<=totalNodes;i++){
    // sorting Algorithm, most probably bubble sort
    Node* temporaryIterator = head;
    while(temporaryIterator!=NULL){
      if (temporaryIterator->next != NULL){
        Node* temp = temporaryIterator->next;
        if (temporaryIterator->time.isBig(temp->time)){
          Node* temp1;
          temp1 = temp->next;
          temp->next = temporaryIterator;
          temporaryIterator->next = temp1;
          if(temporaryIterator->next != head->next){
            head = temp;
          }
        }
      }
      temporaryIterator = temporaryIterator->next;
    }
  }

}

// Function to swap data of two nodes
void swapNodes(Node* a, Node* b) {
    //int tempNumber = a->number;
    SimpleTime tempNumber = a->time;
    //a->number = b->number;
    a->time = b->time;
    b->time = tempNumber;

    char* tempData = a->data;
    a->data = b->data;
    b->data = tempData;
}
// Function to bubble sort the linked list
void bubbleSort(Node* head) {
    bool swapped;
    Node* ptr1;
    Node* lptr = nullptr;

    if (head == nullptr)
        return;

    do {
        swapped = false;
        ptr1 = head;

        while (ptr1->next != lptr) {
            if ( ptr1->time.isBig(ptr1->next->time) ) {
                swapNodes(ptr1, ptr1->next);
                swapped = true;
            }
            ptr1 = ptr1->next;
        }
        lptr = ptr1;
    } while (swapped);
}

// this checks whether the values provided exist in linked list or not. It checks the name, if it exist, checks the time, if time same then return true, if not return false
bool checkValues(char* value, SimpleTime timeValue){
  //int i = 0;
  //Serial.print("checking for values -> ");
  //Serial.print(value);
  //Serial.print(" - ");
  //Serial.print(timeValue.hour);
  //Serial.print(":");
  //Serial.println(timeValue.minute);
  Node* temporaryIterator = head;
  while(temporaryIterator!=NULL){
    //Serial.print("checking ");
    //Serial.println(temporaryIterator->data);
    if( (temporaryIterator->data == value) || strcmp(value, temporaryIterator->data) == 0){   // the value from the info.key was all messed up, this "strcmp" is to counter that mess
      //Serial.println("name matched");
      if(temporaryIterator->time.checkTime(timeValue)){
        //Serial.println("time matched");
        return 1;
      }
    }
    temporaryIterator = temporaryIterator->next;
  }
  //Serial.println("value didn't match");
  return 0;
}

// this checks only the name, and not the time for the linked list, returns true if exist
bool checkOnlyName(char* value){
  Node* temporaryIterator = head;
  while(temporaryIterator!=NULL){
    //Serial.print("checking ");
    //Serial.println(temporaryIterator->data);
    if( (temporaryIterator->data == value) || strcmp(value, temporaryIterator->data) == 0){   // the value from the info.key was all messed up, this "strcmp" is to counter that mess
        return 1;
      }
    temporaryIterator = temporaryIterator->next;
  }
  //Serial.println("value didn't match");
  return 0;
}

// store the string in NVS memory
void store_string(const char* namespace_name, const char* key, const char* value) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        Serial.println("Error opening NVS handle!");
        return;
    }

    err = nvs_set_str(my_handle, key, value);
    if (err != ESP_OK) {
        Serial.println("Failed to write string to NVS!");
    }
    else{
      Serial.println("NVS write successfull");
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        Serial.println("Failed to commit changes to NVS!");
    }

    nvs_close(my_handle);
}

// retrieve the string from nvs memory
void retrieve_string(const char* namespace_name, const char* key) {
    nvs_handle_t my_handle;
    size_t required_size = 0;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        Serial.println("Error opening NVS handle!");
        return;
    }

    err = nvs_get_str(my_handle, key, NULL, &required_size);
    if (err != ESP_OK) {
        Serial.println("Failed to get string size from NVS!");
        nvs_close(my_handle);
        return;
    }

    char* value = (char*) malloc(required_size);
    if (value == NULL) {
        Serial.println("Failed to allocate memory for string");
        nvs_close(my_handle);
        return;
    }

    err = nvs_get_str(my_handle, key, value, &required_size);
    if (err != ESP_OK) {
        Serial.println("Failed to get string from NVS!");
    } else {
        //Serial.print("Retrieved String: ");
        //Serial.println(value);
    }


    // the problem with the code above is that 'value' was not being saved anywhere, and we needed to store it in some global variable
    // therefore, we created a global char pointer that points to null pointer, then inside this function we mallocly pointed to the right amount of memory size
    // and then copied the content of 'value' to that global char

    // Check if globalArray already points to some memory and free it
    if (globalValue != nullptr) {
        free(globalValue);
    }
    // Allocate memory for globalArray and copy the contents
    globalValue = (char*)malloc(strlen(value) + 1); // +1 for null terminator
    if (globalValue != nullptr) {
        strcpy(globalValue, value);
    }

    free(value);
    nvs_close(my_handle);
}

// given the string, this function returns time in SimpleTime format
SimpleTime parseTime(const std::string& data){
  size_t timeIdentifierPos;             // to determine what time is being commanded by phone, sunset/sunrise/custom, note this is memory address position
  std::string timeIdentifier;
  size_t timeIdentifierValuePos;
  std::string timeIdentifierValue;

  timeIdentifierPos = data.find(",");                 // this will get the first comma position, before the first comma position lies the identifier, note this is position not the actual value
  timeIdentifier = data.substr(0,timeIdentifierPos); // this is the actual value that tells whether its sunrise/sunset/custom
  timeIdentifierValuePos = data.find(",", timeIdentifierPos+1);
  timeIdentifierValue = data.substr(timeIdentifierPos+1, (timeIdentifierValuePos-timeIdentifierPos)-1);

  size_t hourPos = timeIdentifierValue.find(":");
  std::string hour = timeIdentifierValue.substr(0, hourPos);
  std::string minute = timeIdentifierValue.substr(hourPos+1, 2);

  SimpleTime time = {std::stoi(hour), std::stoi(minute)};
  //Serial.println("inside parsetime");
  //time.printTime();
  return time;
}


// check if a key exist in the nvs storage or not
bool checkExist(const char* namespace_name, const char* key){
  nvs_handle_t my_handle;
  size_t required_size = 0;
  esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &my_handle);
  if (err != ESP_OK) {
      Serial.println("Error opening NVS handle!");
      return 0;
  }

  err = nvs_get_str(my_handle, key, NULL, &required_size);
  if (err != ESP_OK) {
      Serial.println("Failed to get string size from NVS!");
      nvs_close(my_handle);
      return 0;
  }

  char* value = (char*) malloc(required_size);
  if (value == NULL) {
      Serial.println("Failed to allocate memory for string");
      nvs_close(my_handle);
      return 0;
  }

  err = nvs_get_str(my_handle, key, value, &required_size);
  if (err != ESP_OK) {
      Serial.println("Failed to get string from NVS!");
      return 0;
  } else {
      return 1;
      }

}


// get all the keys in the nvs storage
void listKeys(nvs_handle_t handle) {
  nvs_iterator_t it = nvs_entry_find("nvs", "storage", NVS_TYPE_ANY);
  while (it != NULL) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    //Serial.println("Checking all keys inside NVS storage");
    //Serial.print("Key: ");
    //Serial.println(info.key);
    if (strstr(info.key, "time") != NULL) {
      //Serial.print(info.key);
      //Serial.println(" -> relevant key for scheduling");
      retrieve_string("storage", info.key);     // globalValue now equals the value (from NVS key:value)
      std::string t = globalValue;              // maybe redundant, idk, too lazy to look into it
      SimpleTime scheduleTime = parseTime(t);             // get the time(SimpleTime format) from the string value(from NVS)

      //before adding, we should see if it already exists or not, if it doesn't exist just add, if it does exist, remove previous and add new
      //addNode(info.key, t2);
      globalString = strdup(info.key);          // info.key is char[16] only available in this scope, using it outside the memory address changes
      
      if(checkValues(globalString, scheduleTime)){
        updateNode(globalString, scheduleTime);     // if it exists, update it.. this envelopes both scenarios where (name and time both are same)or(name exist but time is different), updating doesn't hurt any case
        //Serial.print("updated node in linked list: ");
        //Serial.println(info.key);
      }
      else if (checkOnlyName(globalString)){
        updateNode(globalString, scheduleTime);
      }
      else{
        addNode(globalString, scheduleTime);       // value didn't exist before so we add
        //Serial.print("added node to linked list: ");
        //Serial.println(info.key);
      }
    } 
    else {
      //Serial.print(info.key);
      //Serial.println(" -> irrelevant key for scheduling");
    }
    it = nvs_entry_next(it);
  }
  nvs_release_iterator(it);
}


// Data to be parsed is in this format : "sunrise_time,23:00,temp,3400,br,90,fade,160"
void parseData(const std::string& data){
  Serial.println("parseData fuction called, NVS storage about to be updated");
  size_t timeIdentifierPos;             // to determine what time is being commanded by phone, sunset/sunrise/custom, note this is memory address position
  std::string timeIdentifier;
  size_t timeIdentifierValuePos;
  std::string timeIdentifierValue;
  size_t tempValuePos;
  std::string tempValue;
  size_t brValuePos;
  std::string brValue;
  size_t fadeValuePos;
  std::string fadeValue;

  timeIdentifierPos = data.find(",");                 // this will get the first comma position, before the first comma position lies the identifier, note this is position not the actual value
  timeIdentifier = data.substr(0,timeIdentifierPos); // this is the actual value that tells whether its sunrise/sunset/custom
  timeIdentifierValuePos = data.find(",", timeIdentifierPos+1);
  timeIdentifierValue = data.substr(timeIdentifierPos+1, (timeIdentifierValuePos-timeIdentifierPos)-1);
  tempValuePos = data.find(",", timeIdentifierValuePos+6);
  tempValue = data.substr(timeIdentifierValuePos+6, (tempValuePos-(timeIdentifierValuePos+6)));
  brValuePos = data.find(",", tempValuePos+4);
  brValue = data.substr(tempValuePos+4, (brValuePos-(tempValuePos+4)));
  fadeValuePos = data.find(",", brValuePos+6);
  fadeValue = data.substr(brValuePos+6, (fadeValuePos-(brValuePos+6)));


  //const char* cnst = std::to_string(timeIdentifierPos).c_str();         // these 4 commented lines are for debugging, its better if they aren't deleted
  //const char* cnst1 = std::to_string(timeIdentifierValuePos).c_str();
  //Serial.println(cnst);
  //Serial.println(cnst1);

  
  //Serial.println(timeIdentifier.c_str());
  //Serial.println(timeIdentifierValue.c_str());
  //Serial.println(tempValue.c_str());
  //Serial.println(brValue.c_str());
  //Serial.println(fadeValue.c_str());


  if (timeIdentifier == "sunrise_time"){
    Serial.println("sunrise time updated in NVS");
    const char* key = timeIdentifier.c_str();
    store_string("storage", key, data.c_str());
    listKeys(my_handle);
  }

  else if (timeIdentifier == "sunset_time"){
    Serial.println("sunset time updated in NVS");
    const char* key = timeIdentifier.c_str();
    store_string("storage", key, data.c_str());
    listKeys(my_handle);
  }

  else if (strstr(timeIdentifier.c_str(), "time") != NULL){    
    Serial.println("custom time updated in NVS");
    const char* key = timeIdentifier.c_str();
    store_string("storage", key, data.c_str());
    listKeys(my_handle);
  }

  else if (timeIdentifier == "delete"){
    // when deleting, delete both the nvs key and the node in linked_list
    /*err = nvs_erase_key(my_handle, timeIdentifierValue.c_str());  // (handle, key)
    if (err == ESP_OK) {
        Serial.println("Key erased successfully!");
    } else {
        Serial.println("Error erasing key!");
    }*/
  }
}


// Callback class for the characteristic to handle writes
class MyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    //Serial.print(pCharacteristic->getUUID().toString().c_str());
    if (pCharacteristic->getUUID().toString() == "87654321-4321-4321-4321-210987654321"){   // recieves the name of the bulb 
      //Serial.println("inside if statement -----------");
      //set the incoming name as device name
      //defaultBulbName = pCharacteristic->getValue().c_str();
      store_string("storage", "bulb_name", pCharacteristic->getValue().c_str());
    }
    if (pCharacteristic->getUUID().toString() == "f00d4a3e-b447-49d9-b97c-4d7b0360c0ad"){  // sunrise sunset adjustment
      std::string input = pCharacteristic->getValue();
      parseData(input);
    }
    std::string value = pCharacteristic->getValue();
    if (value.empty()) {
      Serial.println("Warning: Write Requested with no value");
    } else {
      Serial.print("New value recieved at UUID : ");
      Serial.println(value.c_str());
    }
  }
};


// prints the whole schedule
void printNodes() {
    Node* temp = head;
    Serial.println("This is the overall schedule");
    while (temp != NULL) {
        Serial.print(temp->data);
        Serial.print("(");
        Serial.print(temp->time.hour);
        Serial.print(":");
        Serial.print(temp->time.minute);
        Serial.print(")");
        Serial.print(" -> ");
        temp = temp->next;
    }
    Serial.println("NULL");
}

// not my code
void updateAdvertisingAndNVS();

// not my code function
void updateAdvertisingAndNVS() {  // everytime data is written on bulb_name characteristic, this function is called for changing device name
    pAdvertising->stop();
    NimBLEDevice::init(currentBulbName);
    NimBLEAdvertisementData newAdvData;
    newAdvData.setName(currentBulbName);
    pAdvertising->setAdvertisementData(newAdvData);
    pAdvertising->start();
    Serial.println("Advertising restarted with updated bulb name: " + String(currentBulbName));

    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvsHandle);
    if (err == ESP_OK) {
        err = nvs_set_str(nvsHandle, "bulb_name", currentBulbName);  // name is stored with key="bulb_name"
        if (err != ESP_OK) {
            Serial.println("Error saving bulb name to NVS");
        }
        nvs_commit(nvsHandle);
        nvs_close(nvsHandle);
    } else {
        Serial.println("Error opening NVS");
    }
}

//esp_err_t err = nvs_flash_init();
//nvs_handle_t my_handle;




void setup() {
  Serial.begin(115200);
  Serial.println("Starting NimBLE Server...");

  err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        size_t size = sizeof(currentBulbName);
        err = nvs_get_str(my_handle, "bulb_name", currentBulbName, &size);
        if (err != ESP_OK) {
            Serial.println("Error loading bulb name from NVS");
            strncpy(currentBulbName, defaultBulbName, sizeof(currentBulbName));
        }
        nvs_close(my_handle);
    } else {
        Serial.println("Error opening NVS");
        strncpy(currentBulbName, defaultBulbName, sizeof(currentBulbName));
    }

  // Initialize NimBLE, no device name given so it will use the default "nimble" name.
  //NimBLEDevice::init("bulb");
  // Create the BLE Server
  //NimBLEServer *pServer = NimBLEDevice::createServer();
  // Create the BLE Service
  //NimBLEService *pService = pServer->createService(SERVICE_UUID);
  
  NimBLEDevice::init(currentBulbName); // not my code
  NimBLEDevice::setSecurityAuth(false, false, true);  // not my code
  NimBLEServer *pServer = NimBLEDevice::createServer();  // not my code
  //pServer->setCallbacks(new MyCallbacks());  // not my code
  NimBLEService* pService = pServer->createService(SERVICE_UUID);  // not my code
  /*
  NimBLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         NIMBLE_PROPERTY::READ
                                       );
  */
  // Create a read-only BLE Characteristic
  NimBLECharacteristic *pBulbNameCharacteristic = pService->createCharacteristic(
                                         BULB_NAME_CHARACTERISTIC_UUID,
                                         NIMBLE_PROPERTY::WRITE
                                       );

  NimBLECharacteristic *pCharacteristicCustom = pService->createCharacteristic(
                                         SUNSET_SETTINGS_CHARACTERISTIC_UUID,
                                         NIMBLE_PROPERTY::WRITE
                                       );

  // Set the value of the characteristic
  //pCharacteristic->setValue("Hi");
  pBulbNameCharacteristic->setValue(currentBulbName); // not my code
  pBulbNameCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristicCustom->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  //NimBLEDevice::startAdvertising();
  NimBLEAdvertisementData initialAdvData;  // not my code
  initialAdvData.setName(currentBulbName); // not my code
  pAdvertising->setAdvertisementData(initialAdvData); // not my code
  pAdvertising->start(); // not my code
  Serial.println("Initial advertising started with bulb name: " + String(currentBulbName)); // not my code
  
  Serial.println("BLE service is up and running!");
  
  // when the device connects with phone get the time on some uuid - TODO
  // that time would be used as global time, for now we assume time as 06:59 - done
  // every loop we calculate millis until we get 1 more minute, then we add to globalTime - done
  // everyloop we check if the next event is about to occur

  //get all the right keys - done
  // add all the right keys to a linked list - done
  // sort the linked list based on the time - done
  // add the current time before next event position - done
  
  //everytime a key added, sort the linked list - done, but i am not sorting at every node addition, i'm sorting every minute

  // if a key is deleted, free the memory on linked list

  // linked list theory:
    // we need to know what the next event should be, therefore, we need to have
    // pointer to our current event
    // pointer to next event
  Serial.print("According to firmware it's ");
  globalTime.printTime();
  listKeys(my_handle);
  printNodes();
  addNode("currentTime", globalTime);
}

void loop() {
  /*
  retrieve_string("storage", "sunrise");  // remember when we retrieve, the global value changes to that retrieved value
  if (globalValue != nullptr) {
      Serial.print("from global ---> ");
      Serial.println(globalValue);
  }
  */
  
  unsigned long currentMillis = millis();
  // as every minute passes, we add 1 minute to the global time
  if (currentMillis - previousMillis >= 60000) {   // 60,000 milliseconds = 60 second = 1 minute       60000
    previousMillis = currentMillis;
    globalTime = globalTime.addMinutes(1);          // add a minute to globalTime
    Serial.println("");
    Serial.print("According to firmware it's ");
    globalTime.printTime();                         // print globalTime
    updateNode("currentTime", globalTime);          // update the globalTime in linked list
    bubbleSort(head);                               // sort the linked list according to new globalTime
    printNodes();                                   // print the whole schedule

    // now we traverse linked list in order to get hold of currentTime pointer
    // once we do that, we check the time remaining from next event
    // once that is done we adjust light values accordingly
    Node* tempiter;     
    tempiter = head;
    while(tempiter!=NULL){
      if(tempiter->data == "currentTime"){   // get the pointer to our current time
        int remainingTimeFromNextEvent = tempiter->time.getRemainingMinutes(tempiter->next->time);   // calculate remaining minutes from next event
        Serial.print("Next scheduled event is ");
        Serial.print(tempiter->next->data);
        Serial.print(" for which ");
        Serial.print(remainingTimeFromNextEvent);
        Serial.println(" minutes are remaining");
        if ( remainingTimeFromNextEvent < 60){   // 60
          // someinterpolationFunction(remainingTimeFromNextEvent)  // we can adjust the values of RGB and brightness according to timeremaining
          Serial.println("less than 60 minutes remaining till next event, adjusting lights accordingly");
          retrieve_string("storage", tempiter->next->data);
          //sunrise_time,23:00,temp,3400,br,90,fade,160
          std::string nextTemp = parseString(globalValue, "temp", "br");
          int nextTempValue = std::stoi(nextTemp);
          currentTemperatureValue = currentTemperatureValue + ( (nextTempValue - currentTemperatureValue)/remainingTimeFromNextEvent );
          Serial.print("Temperature set to : ");
          Serial.println(currentTemperatureValue);
          kelvinToRGB(currentTemperatureValue, red, green, blue);
          // Print the RGB values
          Serial.print("Adjusting RGB values to : ");
          //Serial.print("RGB (");
          Serial.print(red);
          Serial.print(", ");
          Serial.print(green);
          Serial.print(", ");
          Serial.println(blue);

          //std::string nextBr = parseString(globalValue, "br", "fade");
          //Serial.println(nextBr.c_str());
          //int nextBrValue = std::stoi(nextBr);
          //Serial.println(nextBrValue);
          //int z = (nextBrValue - currentBrightnessValue)/remainingTimeFromNextEvent;
          //Serial.println(z);
          //currentBrightnessValue = currentBrightnessValue + ( (nextBrValue - currentBrightnessValue)/remainingTimeFromNextEvent );
          //Serial.print("Brightness set to : ");
          //Serial.println(currentBrightnessValue);


          
          // x = (finalValueOfSomething - currentValueOfSomething)/remainingTimeFromNextEvent
          // currentValueOfSomething = (currentValueOfSomething + x)

        }
      break;
      }
      tempiter = tempiter->next;
    }
  }

}

std::string parseString(char* fullString, char* startSubstr, char* endSubstr){
  //const char *fullString = "dee_40,fade,35,br,65,life,800";
  //const char *startSubstr = "fade";
  //const char *endSubstr = "br";

  char *start = strstr(fullString, startSubstr);
  char *end = strstr(fullString, endSubstr);

  if (start != NULL && end != NULL && end > start) {
    // Calculate the number of characters to copy
    int length = (end-1) - (start + strlen(startSubstr) + 1); // +1 for the comma after "fade"

    if (length > 0) {
      char result[length + 1]; // +1 for null terminator

      // Copy the substring
      strncpy(result, start + strlen(startSubstr) + 1, length);
      result[length] = '\0'; // Null terminate the result string

      //Serial.print("Extracted Substring: ");
      //Serial.println(result);
      std::string str(result);
      return str;
    } 
  } 
}


void kelvinToRGB(int kelvin, int &red, int &green, int &blue) {
  kelvin = constrain(kelvin, 1000, 10000);
  double temp = kelvin / 100.0;

  // Calculate Red
  if (temp <= 66) {
    red = 255;
  } else {
    double r = temp - 60;
    r = 329.698727446 * pow(r, -0.1332047592);
    red = constrain(r, 0, 255);
  }

  // Calculate Green
  if (temp <= 66) {
    double g = temp;
    g = 99.4708025861 * log(g) - 161.1195681661;
    green = constrain(g, 0, 255);
  } else {
    double g = temp - 60;
    g = 288.1221695283 * pow(g, -0.0755148492);
    green = constrain(g, 0, 255);
  }

  // Calculate Blue
  if (temp >= 66) {
    blue = 255;
  } else if (temp <= 19) {
    blue = 0;
  } else {
    double b = temp - 10;
    b = 138.5177312231 * log(b) - 305.0447927307;
    blue = constrain(b, 0, 255);
  }
}







// helper codes
// ---------------> NVS write integer
/*
  err = nvs_open("storage", NVS_READWRITE, &my_handle);  // string, property, &handle
  if (err == ESP_OK) {
    int32_t intVar = 123;
    err = nvs_set_i32(my_handle, "intVar", intVar);   // handle, keyname, valueTobeStored
    if (err == ESP_OK) {
      err = nvs_commit(my_handle);
      if (err == ESP_OK) {
        int32_t intVarRead;
        err = nvs_get_i32(my_handle, "intVar", &intVarRead);
        if (err == ESP_OK) {
          Serial.print("Integer: ");
          Serial.println(intVarRead);
        }
      }
    }
    nvs_close(my_handle);
  }
*/

// ---------------> NVS Read integer
/*
err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err == ESP_OK){
    int32_t intVarRead;
    err = nvs_get_i32(my_handle, "intVar", &intVarRead);
    if (err == ESP_OK) {
      Serial.print("Integer: ");
      Serial.println(intVarRead);
    }
  }
  nvs_close(my_handle);
*/




